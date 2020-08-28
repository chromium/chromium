// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

enum SessionCrashedBubbleHistogramValue {
  SESSION_CRASHED_BUBBLE_SHOWN,
  SESSION_CRASHED_BUBBLE_ERROR,
  SESSION_CRASHED_BUBBLE_RESTORED,
  SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_HELP,
  SESSION_CRASHED_BUBBLE_IGNORED,
  SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN,
  SESSION_CRASHED_BUBBLE_STARTUP_PAGES,
  SESSION_CRASHED_BUBBLE_MAX,
};

void RecordBubbleHistogramValue(SessionCrashedBubbleHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SessionCrashed.Bubble", value, SESSION_CRASHED_BUBBLE_MAX);
}

bool DoesSupportConsentCheck() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

}  // namespace

// A helper class that listens to browser removal event.
class SessionCrashedBubbleView::BrowserRemovalObserver
    : public BrowserListObserver {
 public:
  explicit BrowserRemovalObserver(Browser* browser) : browser_(browser) {
    DCHECK(browser_);
    BrowserList::AddObserver(this);
  }

  ~BrowserRemovalObserver() override { BrowserList::RemoveObserver(this); }

  // Overridden from BrowserListObserver.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_)
      browser_ = nullptr;
  }

  Browser* browser() const { return browser_; }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalObserver);
};

// static
void SessionCrashedBubble::ShowIfNotOffTheRecordProfile(Browser* browser) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser->profile()->IsOffTheRecord())
    return;

  // Observes possible browser removal before Show is called.
  auto browser_observer =
      std::make_unique<SessionCrashedBubbleView::BrowserRemovalObserver>(
          browser);

  if (DoesSupportConsentCheck()) {
    base::PostTaskAndReplyWithResult(
        GoogleUpdateSettings::CollectStatsConsentTaskRunner(), FROM_HERE,
        base::BindOnce(&GoogleUpdateSettings::GetCollectStatsConsent),
        base::BindOnce(&SessionCrashedBubbleView::Show,
                       std::move(browser_observer)));
  } else {
    SessionCrashedBubbleView::Show(std::move(browser_observer), false);
  }
}

// static
void SessionCrashedBubbleView::Show(
    std::unique_ptr<BrowserRemovalObserver> browser_observer,
    bool uma_opted_in_already) {
  // Determine whether or not the UMA opt-in option should be offered. It is
  // offered only when it is a Google chrome build, user hasn't opted in yet,
  // and the preference is modifiable by the user.
  bool offer_uma_optin = false;

  if (DoesSupportConsentCheck() && !uma_opted_in_already)
    offer_uma_optin = !IsMetricsReportingPolicyManaged();

  Browser* browser = browser_observer->browser();

  if (!browser || !browser->tab_strip_model()->GetActiveWebContents()) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAppMenuButton();
  SessionCrashedBubbleView* crash_bubble =
      new SessionCrashedBubbleView(anchor_view, browser, offer_uma_optin);
  views::BubbleDialogDelegateView::CreateBubble(crash_bubble)->Show();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_SHOWN);
  if (uma_opted_in_already)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN);
}

gfx::Size SessionCrashedBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ax::mojom::Role SessionCrashedBubbleView::GetAccessibleWindowRole() {
  return ax::mojom::Role::kAlertDialog;
}

SessionCrashedBubbleView::SessionCrashedBubbleView(views::View* anchor_view,
                                                   Browser* browser,
                                                   bool offer_uma_optin)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      uma_option_(nullptr),
      offer_uma_optin_(offer_uma_optin),
      ignored_(true) {
  DCHECK(anchor_view);

  SetShowCloseButton(true);
  SetTitle(l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, offer_uma_optin_ ? views::CONTROL : views::TEXT));

  // Allow unit tests to leave out Browser.
  const SessionStartupPref session_startup_pref =
      browser_ ? SessionStartupPref::GetStartupPref(browser_->profile())
               : SessionStartupPref{SessionStartupPref::DEFAULT};
  // Offer the option to open the startup pages using the cancel button, but
  // only when the user has selected the URLS option, and set at least one url.
  SetButtons((session_startup_pref.type == SessionStartupPref::URLS &&
              !session_startup_pref.urls.empty())
                 ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
                 : ui::DIALOG_BUTTON_OK);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_STARTUP_PAGES_BUTTON));

  SetAcceptCallback(
      base::BindOnce(&SessionCrashedBubbleView::RestorePreviousSession,
                     base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&SessionCrashedBubbleView::OpenStartupPages,
                                   base::Unretained(this)));

  set_close_on_deactivate(false);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SESSION_CRASHED);
}

SessionCrashedBubbleView::~SessionCrashedBubbleView() {
}

void SessionCrashedBubbleView::OnWidgetDestroying(views::Widget* widget) {
  if (ignored_)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_IGNORED);
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
}

void SessionCrashedBubbleView::Init() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // Description text label.
  auto text_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE),
      CONTEXT_BODY_TEXT_LARGE);
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(text_label));

  if (offer_uma_optin_)
    AddChildView(CreateUmaOptInView());
}

std::unique_ptr<views::View> SessionCrashedBubbleView::CreateUmaOptInView() {
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN);

  // Create a view that will function like a views::Checkbox, but with a
  // StyledLabel instead of the normal Label.
  auto uma_view = std::make_unique<views::View>();
  auto* uma_layout =
      uma_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
  uma_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // The checkbox itself.
  uma_option_ = uma_view->AddChildView(
      std::make_unique<views::Checkbox>(base::string16()));
  uma_option_->SetChecked(false);

  // Move the checkbox border up to |uma_view|.
  uma_view->SetBorder(uma_option_->CreateDefaultBorder());
  uma_option_->SetBorder(nullptr);

  // The text to the right of the checkbox.
  size_t offset;
  base::string16 link_text =
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_UMA_LINK_TEXT);
  base::string16 uma_text = l10n_util::GetStringFUTF16(
      IDS_SESSION_CRASHED_VIEW_UMA_OPTIN,
      link_text,
      &offset);

  auto* uma_label = uma_view->AddChildView(std::make_unique<views::StyledLabel>(
      uma_text, this));
  uma_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                           views::StyledLabel::RangeStyleInfo::CreateForLink());
  views::StyledLabel::RangeStyleInfo uma_style;
  uma_style.text_style = views::style::STYLE_SECONDARY;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    uma_label->AddStyleRange(before_link_range, uma_style);
  gfx::Range after_link_range(offset + link_text.length(), uma_text.length());
  if (!after_link_range.is_empty())
    uma_label->AddStyleRange(after_link_range, uma_style);

  uma_option_->SetAssociatedLabel(uma_label);

  return uma_view;
}

void SessionCrashedBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  browser_->OpenURL(content::OpenURLParams(
      GURL("https://support.google.com/chrome/answer/96817"),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_HELP);
}

void SessionCrashedBubbleView::RestorePreviousSession() {
  ignored_ = false;
  MaybeEnableUma();
  CloseBubble();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_RESTORED);
  // Restoring tabs has side effects, so it's preferable to do it after the
  // bubble was closed.
  SessionRestore::RestoreSessionAfterCrash(browser_);
}

void SessionCrashedBubbleView::OpenStartupPages() {
  ignored_ = false;
  MaybeEnableUma();
  CloseBubble();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_STARTUP_PAGES);
  // Opening tabs has side effects, so it's preferable to do it after the bubble
  // was closed.
  SessionRestore::OpenStartupPagesAfterCrash(browser_);
}

void SessionCrashedBubbleView::MaybeEnableUma() {
  // Record user's choice for opt-in in to UMA.
  // There's no opt-out choice in the crash restore bubble.
  if (uma_option_ && uma_option_->GetChecked()) {
    ChangeMetricsReportingState(true);
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_UMA_OPTIN);
  }
}

void SessionCrashedBubbleView::CloseBubble() {
  GetWidget()->Close();
}
