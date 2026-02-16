// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/geolocation_element_css_fuzzer_grammar.h"
#include "chrome/test/fuzzing/geolocation_element_css_fuzzer_grammar.pb.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_fuzztest_support.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/config/gpu_switches.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// The amount of a pixel's color is allowed to diverge from the target color
// (calculated as Euclidean distance in the RGB space). This value was arrived
// at experimentally.
constexpr int kColorTolerance = 100;

// The amount of expected text/background islands that the element screenshot is
// expected to have. Since the text and its font is hard-set, this values should
// not vary.

// There are 14 text islands, 11 for every letter in "Use location", 1 for the
// "i" dot, and 2 for the element's location pin icon.
constexpr int kExpectedTextIslands = 14;

// There are 6 background islands, 1 for the large island around the text, 1 for
// the location pin icon, and 4 for every letter in "Use location" with a hole.
constexpr int kExpectedBgIslands = 6;

// Helper function to parse rgb(r, g, b) or rgba(r, g, b, a) strings
std::optional<SkColor> ParseRgbString(const std::string& color_str) {
  if (color_str.empty()) {
    return std::nullopt;
  }

  size_t start = color_str.find('(');
  size_t end = color_str.find(')');
  if (start == std::string::npos || end == std::string::npos || end <= start) {
    return std::nullopt;
  }

  std::string values_str = color_str.substr(start + 1, end - start - 1);

  std::vector<std::string> parts = base::SplitString(
      values_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (parts.size() < 3) {
    return std::nullopt;
  }

  std::vector<int> components;
  for (int i = 0; i < 3; ++i) {
    int component;
    if (!base::StringToInt(parts[i], &component) || component < 0 ||
        component > 255) {
      return std::nullopt;
    }
    components.push_back(component);
  }

  return SkColorSetRGB(components[0], components[1], components[2]);
}

// Helper function to check if colors are similar within a tolerance
bool AreColorsSimilar(SkColor c1, SkColor c2, int tolerance) {
  int r_diff =
      static_cast<int>(SkColorGetR(c1)) - static_cast<int>(SkColorGetR(c2));
  int g_diff =
      static_cast<int>(SkColorGetG(c1)) - static_cast<int>(SkColorGetG(c2));
  int b_diff =
      static_cast<int>(SkColorGetB(c1)) - static_cast<int>(SkColorGetB(c2));
  return (r_diff * r_diff + g_diff * g_diff + b_diff * b_diff) <=
         (tolerance * tolerance);
}

// Helper function to count color islands
int CountColorIslands(const SkBitmap& bitmap,
                      SkColor target_color,
                      int tolerance) {
  // Direction vectors for visiting neighbors.
  std::array<int, 8> di = {-1, -1, -1, 0, 0, 1, 1, 1};
  std::array<int, 8> dj = {-1, 0, 1, -1, 1, -1, 0, 1};

  int width = bitmap.width();
  int height = bitmap.height();
  std::vector<std::vector<bool>> visited(height,
                                         std::vector<bool>(width, false));
  int island_count = 0;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      if (visited[i][j]) {
        continue;
      }
      if (!AreColorsSimilar(bitmap.getColor(j, i), target_color, tolerance)) {
        visited[i][j] = true;
        continue;
      }

      std::queue<std::pair<int, int>> q;

      q.emplace(i, j);
      visited[i][j] = true;
      island_count++;

      while (!q.empty()) {
        std::pair<int, int> curr = q.front();
        q.pop();

        for (int k = 0; k < 8; ++k) {
          int next_i = curr.first + di[k];
          int next_j = curr.second + dj[k];

          if (next_i < 0 || next_i >= height || next_j < 0 || next_j >= width) {
            continue;
          }
          if (visited[next_i][next_j]) {
            continue;
          }

          visited[next_i][next_j] = true;
          if (!AreColorsSimilar(bitmap.getColor(next_j, next_i), target_color,
                                tolerance)) {
            continue;
          }
          q.emplace(next_i, next_j);
        }
      }
    }
  }
  return island_count;
}

}  // namespace

class GeolocationElementBrowserFuzzTest
    : public BrowserFuzzTest<InProcessBrowserTest> {
 public:
  using FuzzCase =
      domatolpm::generated::geolocation_element_css_fuzzer_grammar::fuzzcase;

  GeolocationElementBrowserFuzzTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserFuzzTest<InProcessBrowserTest>::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableGpu);
    command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
  }

  void OnScreenshotCaptured(base::OnceClosure quit_closure,
                            SkColor text_color,
                            SkColor bg_color,
                            const content::CopyFromSurfaceResult& result);

  void TestGeolocationTextIsAlwaysReadable(const FuzzCase& fuzz_case);
};

void GeolocationElementBrowserFuzzTest::OnScreenshotCaptured(
    base::OnceClosure quit_closure,
    SkColor text_color,
    SkColor bg_color,
    const content::CopyFromSurfaceResult& result) {
  base::ScopedClosureRunner closer(std::move(quit_closure));

  if (!result.has_value()) {
    NOTREACHED() << "Screenshot failed";
  }

  const SkBitmap& bitmap = result->bitmap;

  if (bitmap.height() == 0 || bitmap.width() == 0) {
    NOTREACHED() << "Bitmap is empty";
  }

  // This is the main goal of the test. We use a simple island counting
  // algorithm to count the number of islands matching background/text color
  // respectively and check and it matches the expected amounts if the text was
  // fully visible. This relies on the geolocation element text and font being
  // fixed.
  int text_islands = CountColorIslands(bitmap, text_color, kColorTolerance);
  int bg_islands = CountColorIslands(bitmap, bg_color, kColorTolerance);

  EXPECT_EQ(text_islands, kExpectedTextIslands);
  EXPECT_EQ(bg_islands, kExpectedBgIslands);
}

void GeolocationElementBrowserFuzzTest::TestGeolocationTextIsAlwaysReadable(
    const FuzzCase& fuzz_case) {
  domatolpm::Context ctx;
  CHECK(domatolpm::geolocation_element_css_fuzzer_grammar::handle_fuzzer(
      &ctx, fuzz_case));
  std::string html_content(ctx.GetBuilder()->view());

  // See
  // docs/security/url_display_guidelines/url_display_guidelines.md#url-length
  const size_t kMaxUrlLength = 2 * 1024 * 1024;
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(html_content, kUsePlus));
  if (url_string.length() > kMaxUrlLength) {
    return;
  }

  if (!ui_test_utils::NavigateToURL(browser(), GURL(url_string))) {
    NOTREACHED() << "Failed to load page";
  }
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    NOTREACHED() << "WebContents is null after navigation";
  }
  if (!content::WaitForLoadStop(web_contents)) {
    NOTREACHED() << "Failed to wait for page load stop";
  }

  auto* rfh = web_contents->GetPrimaryMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    NOTREACHED() << "RenderFrameHost is not live after navigation";
  }

  // Get element bounds and styles, after the element is valid.
  std::string script = R"(waitForElementAndGetData();)";
  content::EvalJsResult result = content::EvalJs(rfh, script);
  if (!result.is_ok()) {
    NOTREACHED() << "Failed to get element data: " << result.ExtractError();
  }

  if (!result.is_dict()) {
    // Gracefully exit this fuzz case, this is an expected possibility.
    LOG(INFO) << "Element did not become valid";
    return;
  }

  const base::DictValue& dict = result.ExtractDict();

  const std::string* error_string = dict.FindString("error");
  if (error_string) {
    NOTREACHED() << "Javascript returned an error: " << *error_string;
  }

  const base::DictValue* element_dict = dict.FindDict("element");
  if (!element_dict) {
    NOTREACHED() << "Failed to find 'element' key in result: " << dict;
  }

  std::optional<int> x = element_dict->FindInt("x");
  std::optional<int> y = element_dict->FindInt("y");
  std::optional<int> width = element_dict->FindInt("width");
  std::optional<int> height = element_dict->FindInt("height");

  if (!x || !y || !width || !height || *width <= 0 || *height <= 0) {
    NOTREACHED() << "Invalid element bounds: " << *element_dict;
  }

  gfx::Rect element_bounds(*x, *y, *width, *height);

  const base::DictValue* style_dict = dict.FindDict("style");
  if (!style_dict) {
    NOTREACHED() << "Style dictionary is not present in response";
  }

  const std::string* text_color_str = style_dict->FindString("color");
  const std::string* bg_color_str = style_dict->FindString("backgroundColor");
  if (!text_color_str || !bg_color_str) {
    NOTREACHED() << "Colors not available in response";
  }

  std::optional<SkColor> text_color = ParseRgbString(*text_color_str);
  std::optional<SkColor> bg_color = ParseRgbString(*bg_color_str);

  if (!text_color || !bg_color) {
    NOTREACHED() << "Failed to parse colors: " << *text_color_str << ", "
                 << *bg_color_str;
  }

  // Take screenshot of the element to run heuristic on.
  base::RunLoop run_loop;
  web_contents->GetPrimaryMainFrame()->GetView()->CopyFromSurface(
      element_bounds, gfx::Size(), base::TimeDelta(),
      base::BindOnce(&GeolocationElementBrowserFuzzTest::OnScreenshotCaptured,
                     base::Unretained(this), run_loop.QuitClosure(),
                     *text_color, *bg_color));
  run_loop.Run();
}

FUZZ_TEST_F(GeolocationElementBrowserFuzzTest,
            TestGeolocationTextIsAlwaysReadable);
