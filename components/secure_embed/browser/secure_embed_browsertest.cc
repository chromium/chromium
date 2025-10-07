// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "components/secure_embed/browser/secure_embed_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace secure_embed {

namespace {
constexpr char kTestUrl[] = "/secure_embed/embed_tag.html";
}  // namespace

class SecureEmbedBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    // Enable pixel output in tests to allow CopyFromSurface to capture
    // actual rendered content instead of returning empty/black bitmaps.
    // TODO(secure-embed): Remove this if this test class stops using pixel
    // verification.
    command_line->AppendSwitch("--enable-pixel-output-in-tests");
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void NavigateToTestUrl(const char* url) {
    const GURL test_url = embedded_test_server()->GetURL(url);
    ASSERT_TRUE(NavigateToURL(web_contents(), test_url));
  }

  int CountEmbedElementsInPage() {
    return content::EvalJs(web_contents(), "document.embeds.length")
        .ExtractInt();
  }

  bool WaitForHostCreation(size_t expected_count) {
    // Host creation is asynchronous because it requires mojo IPC between
    // the renderer process (SecureEmbedWebPlugin) and browser process
    // (SecureEmbedHost). Poll until the expected number of hosts are created.
    return base::test::RunUntil([&]() {
      return SecureEmbedHost::GetInstanceCountForTesting() >= expected_count;
    });
  }
};

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, EmbedTagCreatesPlugin) {
  NavigateToTestUrl(kTestUrl);

  EXPECT_EQ(1, CountEmbedElementsInPage());

  ASSERT_TRUE(WaitForHostCreation(1));
  EXPECT_EQ(1u, SecureEmbedHost::GetInstanceCountForTesting());
}

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, PluginRendersRedSquare) {
  // TODO(secure-embed): This test currently just verifies that the plugin
  // renders something red in the expected location. As the SecureEmbedWebPlugin
  // is developed further, this test should be updated to verify that the
  // correct content is rendered or removed completely if another verification
  // method is implemented.
  NavigateToTestUrl(kTestUrl);

  ASSERT_TRUE(WaitForHostCreation(1));

  auto* rwhv = web_contents()->GetRenderWidgetHostView();
  ASSERT_NE(rwhv, nullptr);

  // Poll for red content in the bitmap. We capture and check repeatedly
  // because the plugin may not have painted yet when the first frame is
  // submitted during page load. Grab a small rect to query.
  gfx::Rect capture_rect(95, 95, 5, 5);

  bool pixel_is_red = false;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    base::RunLoop run_loop;

    rwhv->CopyFromSurface(capture_rect, capture_rect.size(),
                          base::BindOnce(
                              [](base::OnceClosure quit_closure, bool* result,
                                 const SkBitmap& bitmap) {
                                *result = false;

                                if (bitmap.drawsNothing()) {
                                  std::move(quit_closure).Run();
                                  return;
                                }

                                SkColor color = bitmap.getColor(0, 0);
                                if (color == SK_ColorRED) {
                                  *result = true;
                                }

                                std::move(quit_closure).Run();
                              },
                              run_loop.QuitClosure(), &pixel_is_red));

    run_loop.Run();
    return pixel_is_red;
  }));
}

}  // namespace secure_embed
