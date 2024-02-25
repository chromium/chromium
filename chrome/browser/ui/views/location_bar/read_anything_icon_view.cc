// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/read_anything_icon_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace {

// The time, in seconds, which the label is shown, before animating out again.
// This number was chosen to be long enough for target users, who are slower
// readers, to read the label.
constexpr auto kLabelShownDuration = base::Seconds(3);

}  // namespace

ReadAnythingIconView::ReadAnythingIconView(
    CommandUpdater* command_updater,
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SHOW_READING_MODE_SIDE_PANEL,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "ReadAnythingIcon",
                         true),
      label_shown_count_(browser->profile()->GetPrefs()->GetInteger(
          prefs::kAccessibilityReadAnythingOmniboxIconLabelShownCount)),
      browser_(browser) {
  DCHECK(browser_);

  SetActive(false);
  SetUpForInOutAnimation(kLabelShownDuration);

  coordinator_ = ReadAnythingCoordinator::FromBrowser(browser_);
  if (coordinator_) {
    coordinator_observer_.Observe(coordinator_);
  }
}

ReadAnythingIconView::~ReadAnythingIconView() = default;

void ReadAnythingIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  ShowReadAnythingSidePanel(browser_,
                            SidePanelOpenTrigger::kReadAnythingOmniboxIcon);
}

views::BubbleDialogDelegate* ReadAnythingIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& ReadAnythingIconView::GetVectorIcon() const {
  return kMenuBookChromeRefreshIcon;
}

void ReadAnythingIconView::UpdateImpl() {
  if (GetVisible() != should_be_visible_) {
    SetVisible(should_be_visible_);
    base::UmaHistogramBoolean("Accessibility.ReadAnything.OmniboxIconShown",
                              should_be_visible_);
  }
  if (!should_be_visible_ || is_animating_label() ||
      (label_shown_count_ >= kReadAnythingOmniboxIconLabelShownCountMax)) {
    return;
  }
  ResetSlideAnimation(false);
  AnimateIn(IDS_READING_MODE_TITLE);
  ++label_shown_count_;
  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingOmniboxIconLabelShownCount,
      label_shown_count_);
}

void ReadAnythingIconView::Activate(bool active) {
  if (active) {
    should_be_visible_ = false;
    Update();
    base::UmaHistogramBoolean("Accessibility.ReadAnything.OmniboxIconShown",
                              false);
  }
}

void ReadAnythingIconView::OnCoordinatorDestroyed() {
  coordinator_ = nullptr;
}

void ReadAnythingIconView::OnActivePageDistillable(bool distillable) {
  if (IsReadAnythingEntryShowing(browser_)) {
    return;
  }
  should_be_visible_ = distillable;
  Update();
}

BEGIN_METADATA(ReadAnythingIconView)
END_METADATA
