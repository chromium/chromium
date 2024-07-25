// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/ui_metrics/sadtab_metrics_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/memory/oom_memory_details.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

namespace {

void RecordEvent(bool feedback, ui_metrics::SadTabEvent event) {
  if (feedback) {
    UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabFeedbackHistogramKey, event,
                              ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabReloadHistogramKey, event,
                              ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
  }
}

constexpr char kCategoryTagCrash[] = "Crash";

// Return true if this function has been called in the last 10 seconds.
bool IsRepeatedlyCrashing() {
  const int kMaxSecondsSinceLastCrash = 10;

  static int64_t last_called_ts = 0;
  base::TimeTicks last_called(base::TimeTicks::UnixEpoch());

  if (last_called_ts)
    last_called = base::TimeTicks::FromInternalValue(last_called_ts);

  bool crashed_recently = (base::TimeTicks().Now() - last_called).InSeconds() <
                          kMaxSecondsSinceLastCrash;

  last_called_ts = base::TimeTicks().Now().ToInternalValue();
  return crashed_recently;
}

bool AreOtherTabsOpen() {
  size_t tab_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    tab_count += browser->tab_strip_model()->count();
    if (tab_count > 1U)
      break;
  }
  return (tab_count > 1U);
}

}  // namespace

// static
bool SadTab::ShouldShow(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
    case base::TERMINATION_STATUS_OOM:
      return true;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TERMINATION_STATUS_MAX_ENUM:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

int SadTab::GetTitle() {
  if (!is_repeatedly_crashing_)
    return IDS_SAD_TAB_TITLE;
  switch (kind_) {
#if BUILDFLAG(IS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      return IDS_SAD_TAB_RELOAD_TITLE;
#endif
    case SAD_TAB_KIND_OOM:
#if BUILDFLAG(IS_WIN)  // Only Windows has OOM sad tab strings.
      return IDS_SAD_TAB_OOM_TITLE;
#endif
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_KILLED:
      return IDS_SAD_TAB_RELOAD_TITLE;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

int SadTab::GetErrorCodeFormatString() {
  return IDS_SAD_TAB_ERROR_CODE;
}

int SadTab::GetInfoMessage() {
  switch (kind_) {
#if BUILDFLAG(IS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      return IDS_KILLED_TAB_BY_OOM_MESSAGE;
#endif
    case SAD_TAB_KIND_OOM:
      if (is_repeatedly_crashing_)
        return AreOtherTabsOpen() ? IDS_SAD_TAB_OOM_MESSAGE_TABS
                                  : IDS_SAD_TAB_OOM_MESSAGE_NOTABS;
      return IDS_SAD_TAB_MESSAGE;
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_KILLED:
      return is_repeatedly_crashing_ ? IDS_SAD_TAB_RELOAD_TRY
                                     : IDS_SAD_TAB_MESSAGE;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

int SadTab::GetButtonTitle() {
  return show_feedback_button_ ? IDS_CRASHED_TAB_FEEDBACK_LINK
                               : IDS_SAD_TAB_RELOAD_LABEL;
}

int SadTab::GetHelpLinkTitle() {
  return IDS_LEARN_MORE;
}

const char* SadTab::GetHelpLinkURL() {
  return show_feedback_button_ ? chrome::kCrashReasonFeedbackDisplayedURL
                               : chrome::kCrashReasonURL;
}

std::vector<int> SadTab::GetSubMessages() {
  if (!is_repeatedly_crashing_)
    return std::vector<int>();

  switch (kind_) {
#if BUILDFLAG(IS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      return std::vector<int>();
#endif
    case SAD_TAB_KIND_OOM:
      return std::vector<int>();
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_KILLED:
      std::vector<int> message_ids = {IDS_SAD_TAB_RELOAD_RESTART_BROWSER,
                                      IDS_SAD_TAB_RELOAD_RESTART_DEVICE};
      // Only show Incognito suggestion if not already in Incognito mode.
      if (!web_contents_->GetBrowserContext()->IsOffTheRecord())
        message_ids.insert(message_ids.begin(), IDS_SAD_TAB_RELOAD_INCOGNITO);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // Note: on macOS, Linux and ChromeOS, the first bullet is either one of
      // IDS_SAD_TAB_RELOAD_CLOSE_TABS or IDS_SAD_TAB_RELOAD_CLOSE_NOTABS
      // followed by one of the above suggestions.
      message_ids.insert(message_ids.begin(),
                         AreOtherTabsOpen() ? IDS_SAD_TAB_RELOAD_CLOSE_TABS
                                            : IDS_SAD_TAB_RELOAD_CLOSE_NOTABS);
#endif
      return message_ids;
  }
  NOTREACHED_IN_MIGRATION();
  return std::vector<int>();
}

int SadTab::GetCrashedErrorCode() {
  return web_contents_->GetCrashedErrorCode();
}

void SadTab::RecordFirstPaint() {
  DCHECK(!recorded_paint_);
  recorded_paint_ = true;

  RecordEvent(show_feedback_button_, ui_metrics::SadTabEvent::DISPLAYED);
}

void SadTab::PerformAction(SadTab::Action action) {
  DCHECK(recorded_paint_);
  switch (action) {
    case Action::BUTTON:
      RecordEvent(show_feedback_button_,
                  ui_metrics::SadTabEvent::BUTTON_CLICKED);
      if (show_feedback_button_) {
        chrome::ShowFeedbackPage(
            chrome::FindBrowserWithTab(web_contents_),
            feedback::kFeedbackSourceSadTabPage,
            std::string() /* description_template */,
            l10n_util::GetStringUTF8(kind_ == SAD_TAB_KIND_CRASHED
                                         ? IDS_CRASHED_TAB_FEEDBACK_MESSAGE
                                         : IDS_KILLED_TAB_FEEDBACK_MESSAGE),
            std::string(kCategoryTagCrash), std::string());
      } else {
        web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                              true);
      }
      break;
    case Action::HELP_LINK:
      RecordEvent(show_feedback_button_,
                  ui_metrics::SadTabEvent::HELP_LINK_CLICKED);
      content::OpenURLParams params(GURL(GetHelpLinkURL()), content::Referrer(),
                                    WindowOpenDisposition::CURRENT_TAB,
                                    ui::PAGE_TRANSITION_LINK, false);
      web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
      break;
  }
}

SadTab::SadTab(content::WebContents* web_contents, SadTabKind kind)
    : web_contents_(web_contents),
      kind_(kind),
      is_repeatedly_crashing_(IsRepeatedlyCrashing()),
      show_feedback_button_(false),
      recorded_paint_(false) {
  switch (kind) {
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_OOM:
      break;
#if BUILDFLAG(IS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM: {
      const std::string spec =
          web_contents->GetURL().DeprecatedGetOriginAsURL().spec();
      memory::OomMemoryDetails::Log("Tab OOM-Killed Memory details: " + spec +
                                    ", ");
      [[fallthrough]];
    }
#endif
    case SAD_TAB_KIND_KILLED:
      LOG(WARNING) << "Tab Killed: "
                   << web_contents->GetURL().DeprecatedGetOriginAsURL().spec();
      break;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Sending feedback is not allowed in the ChromeOS Kiosk mode.
  if (chromeos::IsKioskSession()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Only Google Chrome-branded browsers may show the Feedback button.
  show_feedback_button_ = is_repeatedly_crashing_;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
