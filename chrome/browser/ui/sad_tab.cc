// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/ui_metrics/sadtab_metrics_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/memory/oom_memory_details.h"
#endif

namespace {

// These stats should use the same counting approach and bucket size as tab
// discard events in memory::OomPriorityManager so they can be directly
// compared.

// This macro uses a static counter to track how many times it's hit in a
// session. See Tabs.SadTab.CrashCreated in histograms.xml for details.
#define UMA_SAD_TAB_COUNTER(histogram_name)           \
  {                                                   \
    static int count = 0;                             \
    ++count;                                          \
    UMA_HISTOGRAM_COUNTS_1000(histogram_name, count); \
  }

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

bool ShouldShowFeedbackButton() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const int kMinSecondsBetweenCrashesForFeedbackButton = 10;

  static int64_t last_called_ts = 0;
  base::TimeTicks last_called(base::TimeTicks::UnixEpoch());

  if (last_called_ts)
    last_called = base::TimeTicks::FromInternalValue(last_called_ts);

  bool should_show = (base::TimeTicks().Now() - last_called).InSeconds() <
                     kMinSecondsBetweenCrashesForFeedbackButton;

  last_called_ts = base::TimeTicks().Now().ToInternalValue();
  return should_show;
#else
  return false;
#endif
}

bool AreOtherTabsOpen() {
  size_t tab_count = 0;
  for (auto* browser : *BrowserList::GetInstance())
    tab_count += browser->tab_strip_model()->count();
  return (tab_count > 1U);
}

}  // namespace

// static
bool SadTab::ShouldShow(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
    case base::TERMINATION_STATUS_OOM:
      return true;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TERMINATION_STATUS_MAX_ENUM:
      return false;
  }
  NOTREACHED();
  return false;
}

int SadTab::GetTitle() {
  if (!show_feedback_button_)
    return IDS_SAD_TAB_TITLE;
  switch (kind_) {
#if defined(OS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      return IDS_SAD_TAB_RELOAD_TITLE;
#endif
    case SAD_TAB_KIND_OOM:
#if defined(OS_WIN)  // Only Windows has OOM sad tab strings.
      return IDS_SAD_TAB_OOM_TITLE;
#endif
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_KILLED:
      return IDS_SAD_TAB_RELOAD_TITLE;
  }
  NOTREACHED();
  return 0;
}

int SadTab::GetInfoMessage() {
  switch (kind_) {
#if defined(OS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      return IDS_KILLED_TAB_BY_OOM_MESSAGE;
#endif
    case SAD_TAB_KIND_OOM:
      if (show_feedback_button_)
        return AreOtherTabsOpen() ? IDS_SAD_TAB_OOM_MESSAGE_TABS
                                  : IDS_SAD_TAB_OOM_MESSAGE_NOTABS;
      return IDS_SAD_TAB_MESSAGE;
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_KILLED:
      return show_feedback_button_ ? IDS_SAD_TAB_RELOAD_TRY
                                   : IDS_SAD_TAB_MESSAGE;
  }
  NOTREACHED();
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
  if (!show_feedback_button_)
    return std::vector<int>();

  switch (kind_) {
#if defined(OS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      return std::vector<int>();
#endif
    case SAD_TAB_KIND_OOM:
      return std::vector<int>();
    case SAD_TAB_KIND_CRASHED:
    case SAD_TAB_KIND_KILLED:
      std::vector<int> message_ids = {IDS_SAD_TAB_RELOAD_RESTART_BROWSER,
                                      IDS_SAD_TAB_RELOAD_RESTART_DEVICE};
      // Only show incognito suggestion if not already in Incognito mode.
      if (!web_contents_->GetBrowserContext()->IsOffTheRecord())
        message_ids.insert(message_ids.begin(), IDS_SAD_TAB_RELOAD_INCOGNITO);
#if defined(OS_MACOSX) || defined(OS_LINUX)
      // Note: on macOS, Linux and ChromeOS, the first bullet is either one of
      // IDS_SAD_TAB_RELOAD_CLOSE_TABS or IDS_SAD_TAB_RELOAD_CLOSE_NOTABS
      // followed by one of the above suggestions.
      message_ids.insert(message_ids.begin(),
                         AreOtherTabsOpen() ? IDS_SAD_TAB_RELOAD_CLOSE_TABS
                                            : IDS_SAD_TAB_RELOAD_CLOSE_NOTABS);
#endif
      return message_ids;
  }
  NOTREACHED();
  return std::vector<int>();
}

void SadTab::RecordFirstPaint() {
  DCHECK(!recorded_paint_);
  recorded_paint_ = true;

  switch (kind_) {
    case SAD_TAB_KIND_CRASHED:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.CrashDisplayed");
      break;
    case SAD_TAB_KIND_OOM:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.OomDisplayed");
      break;
#if defined(OS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.KillDisplayed.OOM");
      FALLTHROUGH;
#endif
    case SAD_TAB_KIND_KILLED:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.KillDisplayed");
      break;
  }

  RecordEvent(show_feedback_button_, ui_metrics::SadTabEvent::DISPLAYED);
}

void SadTab::PerformAction(SadTab::Action action) {
  DCHECK(recorded_paint_);
  switch (action) {
    case Action::BUTTON:
      RecordEvent(show_feedback_button_,
                  ui_metrics::SadTabEvent::BUTTON_CLICKED);
      if (show_feedback_button_) {
        ShowFeedbackPage(
            chrome::FindBrowserWithWebContents(web_contents_),
            chrome::kFeedbackSourceSadTabPage,
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
      web_contents_->OpenURL(params);
      break;
  }
}

SadTab::SadTab(content::WebContents* web_contents, SadTabKind kind)
    : web_contents_(web_contents),
      kind_(kind),
      show_feedback_button_(ShouldShowFeedbackButton()),
      recorded_paint_(false) {
  switch (kind) {
    case SAD_TAB_KIND_CRASHED:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.CrashCreated");
      break;
    case SAD_TAB_KIND_OOM:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.OomCreated");
      break;
#if defined(OS_CHROMEOS)
    case SAD_TAB_KIND_KILLED_BY_OOM:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.KillCreated.OOM");
      {
        const std::string spec = web_contents->GetURL().GetOrigin().spec();
        memory::OomMemoryDetails::Log("Tab OOM-Killed Memory details: " + spec +
                                      ", ");
      }
      FALLTHROUGH;
#endif
    case SAD_TAB_KIND_KILLED:
      UMA_SAD_TAB_COUNTER("Tabs.SadTab.KillCreated");
      LOG(WARNING) << "Tab Killed: "
                   << web_contents->GetURL().GetOrigin().spec();
      break;
  }
}
