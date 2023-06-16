// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/reader_mode/reader_mode_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/dom_distiller/content/browser/uma_helper.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"

using dom_distiller::UMAHelper;
using dom_distiller::url_utils::IsDistilledPage;

namespace {
UMAHelper::ReaderModePageType GetPageType(content::WebContents* contents) {
  // Determine if the current web contents is a distilled page.
  UMAHelper::ReaderModePageType page_type =
      UMAHelper::ReaderModePageType::kNone;
  if (IsDistilledPage(contents->GetLastCommittedURL())) {
    page_type = UMAHelper::ReaderModePageType::kDistilled;
  } else {
    absl::optional<dom_distiller::DistillabilityResult> distillability =
        dom_distiller::GetLatestResult(contents);
    if (distillability && distillability.value().is_distillable)
      page_type = UMAHelper::ReaderModePageType::kDistillable;
  }
  return page_type;
}

}  // namespace

ReaderModeIconView::ReaderModeIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    PrefService* pref_service)
    : PageActionIconView(command_updater,
                         IDC_DISTILL_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "ReaderMode"),
      pref_service_(pref_service) {
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(GetActive() ? IDS_EXIT_DISTILLED_PAGE
                                            : IDS_DISTILL_PAGE));
}

ReaderModeIconView::~ReaderModeIconView() {
  content::WebContents* contents = web_contents();
  if (contents)
    dom_distiller::RemoveObserver(contents, this);
  DCHECK(!DistillabilityObserver::IsInObserverList());
}

void ReaderModeIconView::PrimaryPageChanged(content::Page& page) {
  if (GetVisible())
    views::InkDrop::Get(this)->AnimateToState(views::InkDropState::HIDDEN,
                                              nullptr);
}

void ReaderModeIconView::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = GetWebContents();
  // Only primary main frame navigations are relevant for tracking time stats.
  if (!navigation_handle->IsInPrimaryMainFrame() || !web_contents)
    return;
  // When navigation is about to happen, ensure timers are appropriately stopped
  // and reset.
  UMAHelper::UpdateTimersOnNavigation(GetWebContents(),
                                      GetPageType(web_contents));
}

void ReaderModeIconView::PrimaryMainDocumentElementAvailable() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  UMAHelper::StartTimerIfNeeded(web_contents, GetPageType(web_contents));
}

void ReaderModeIconView::UpdateImpl() {
  content::WebContents* contents = GetWebContents();
  if (!contents) {
    SetVisible(false);
    return;
  }
  UMAHelper::ReaderModePageType page_type = GetPageType(contents);

  // WebContentsObserver::web_contents() is not updated until the call to
  // Observe() below, so it should still contain the old contents. This will
  // be used to ensure the timers for UMA are updated for the old web contents.
  content::WebContents* old_contents = web_contents();
  if (contents != old_contents)
    UMAHelper::UpdateTimersOnContentsChange(contents, old_contents);

  if (page_type == UMAHelper::ReaderModePageType::kDistilled) {
    SetVisible(true);
    SetActive(true);
  } else {
    // If the reader mode option shouldn't be shown to the user per their pref
    // in appearance settings, simply hide the icon.
    // TODO(katie): In this case, we should not even check if a page is
    // distillable.
    if (!dom_distiller::ShowReaderModeOption(pref_service_)) {
      SetVisible(false);
      return;
    }
    // If the currently active web contents has changed since last time, stop
    // observing the old web contents and start observing the new one.
    if (old_contents != contents) {
      if (old_contents)
        dom_distiller::RemoveObserver(old_contents, this);
      dom_distiller::AddObserver(contents, this);
    }
    SetVisible(page_type == UMAHelper::ReaderModePageType::kDistillable);
    SetActive(false);
  }

  SetAccessibleName(l10n_util::GetStringUTF16(
      GetActive() ? IDS_EXIT_DISTILLED_PAGE : IDS_DISTILL_PAGE));

  // Notify the icon when navigation to and from a distilled page occurs so that
  // it can hide the inkdrop.
  Observe(contents);
}

const gfx::VectorIcon& ReaderModeIconView::GetVectorIcon() const {
  if (OmniboxFieldTrial::IsChromeRefreshIconsEnabled()) {
    return GetActive() ? kReaderModeRefreshIcon
                       : kReaderModeDisabledRefreshIcon;
  }

  return GetActive() ? kReaderModeIcon : kReaderModeDisabledIcon;
}

// TODO(gilmanmh): Consider displaying a bubble the first time a user
// activates the icon to explain what Reader Mode is.
views::BubbleDialogDelegate* ReaderModeIconView::GetBubble() const {
  return nullptr;
}

void ReaderModeIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (GetActive()) {
    dom_distiller::UMAHelper::RecordReaderModeExit(
        dom_distiller::UMAHelper::ReaderModeEntryPoint::kOmniboxIcon);
  } else {
    dom_distiller::UMAHelper::RecordReaderModeEntry(
        dom_distiller::UMAHelper::ReaderModeEntryPoint::kOmniboxIcon);
  }

  content::WebContents* contents = GetWebContents();
  if (!contents || IsDistilledPage(contents->GetLastCommittedURL()))
    return;
  ukm::SourceId source_id =
      contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::ReaderModeActivated(source_id)
      .SetActivatedViaOmnibox(true)
      .Record(ukm::UkmRecorder::Get());
}

void ReaderModeIconView::OnResult(
    const dom_distiller::DistillabilityResult& result) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    Update();
    return;
  }

  if (result.is_last) {
    ukm::SourceId source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    ukm::builders::ReaderModeReceivedDistillability(source_id)
        .SetIsPageDistillable(result.is_distillable)
        .Record(ukm::UkmRecorder::Get());
  }

  Update();

  // Once we know the type of page we are on (distillable or not), we can
  // update the timers.
  UMAHelper::ReaderModePageType page_type = GetPageType(web_contents);
  UMAHelper::StartTimerIfNeeded(web_contents, page_type);
}

BEGIN_METADATA(ReaderModeIconView, PageActionIconView)
END_METADATA
