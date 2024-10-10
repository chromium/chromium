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

void NavigateToDataUrl(
    content::WebContents* web_contents,
    const QuickAnswersBrowserTestBase::ShowMenuParams& params) {
  const std::string text =
      params.is_password_field
          ? base::StrCat(
                {"<input type=\"password\">", params.selected_text, "</input>"})
          : params.selected_text;
  const std::string data_url =
      base::StringPrintf(kDataUrlTemplate, params.x, params.y, text.c_str());
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(data_url)));
}

void RightClick(content::WebContents* web_contents,
                const QuickAnswersBrowserTestBase::ShowMenuParams& params) {
  // Right click on a word shows a context menu with selecting it.
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(params.x, params.y));
}

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

void QuickAnswersBrowserTestBase::ShowMenu(
    const QuickAnswersBrowserTestBase::ShowMenuParams& params) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  NavigateToDataUrl(web_contents, params);
  RightClick(web_contents, params);
}

void QuickAnswersBrowserTestBase::ShowMenuAndWait(
    const QuickAnswersBrowserTestBase::ShowMenuParams& params) {
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  NavigateToDataUrl(web_contents, params);

  content::ContextMenuInterceptor context_menu_interceptor(
      web_contents->GetPrimaryMainFrame());
  RightClick(web_contents, params);
  context_menu_interceptor.Wait();
}

}  // namespace quick_answers
