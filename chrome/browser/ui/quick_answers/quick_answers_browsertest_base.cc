// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_browsertest_base.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace quick_answers {
namespace {

constexpr char kDataUrlTemplate[] =
    "data:text/html,<html><body><span style=\"position: absolute; left: %ipx; "
    "top: %ipx;\">%s</body></html>";

}  // namespace

QuickAnswersBrowserTestBase::QuickAnswersBrowserTestBase() {
  // Note that `kMahi` is associated with the Magic Boost feature.
  scoped_feature_list_.InitWithFeatureStates(
      {{chromeos::features::kMahi, IsMagicBoostEnabled()},
       {chromeos::features::kFeatureManagementMahi, IsMagicBoostEnabled()}});

  if (IsMagicBoostEnabled()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kMahiRestrictionsOverride);
  }
}

QuickAnswersBrowserTestBase::~QuickAnswersBrowserTestBase() = default;

void QuickAnswersBrowserTestBase::SetUpOnMainThread() {
  QuickAnswersState::Get()->SetEligibilityForTesting(true);
}

bool QuickAnswersBrowserTestBase::IsMagicBoostEnabled() const {
  return GetParam();
}

// `ShowMenu` generates a web page with `params.selected_text` at a position of
// (`params.x`, `params.y`) and right click on it.
void QuickAnswersBrowserTestBase::ShowMenu(
    const QuickAnswersBrowserTestBase::ShowMenuParams& params) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  const std::string text =
      params.is_password_field
          ? base::StrCat(
                {"<input type=\"password\">", params.selected_text, "</input>"})
          : params.selected_text;
  const std::string data_url =
      base::StringPrintf(kDataUrlTemplate, params.x, params.y, text.c_str());
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(data_url)));

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  // Right click on a word shows a context menu with selecting it.
  content::ContextMenuInterceptor context_menu_interceptor(main_frame);
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(params.x, params.y));

  // Wait until the context menu is shown. Note that this only waits context
  // menu. Quick answers might require additional async operations before it's
  // shown.
  context_menu_interceptor.Wait();
}

}  // namespace quick_answers
