// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/policy/idle_dialog_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

static constexpr int kLabelSpacing = 4;

std::unique_ptr<views::Label> CreateLabel() {
  auto label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(kLabelSpacing, 0)));
  return label;
}

}  // namespace

// static
base::WeakPtr<views::Widget> IdleDialog::Show(
    Browser* browser,
    base::TimeDelta dialog_duration,
    base::TimeDelta idle_threshold,
    IdleDialog::ActionSet actions,
    base::OnceClosure on_close_by_user) {
  return policy::IdleDialogView::Show(browser, dialog_duration, idle_threshold,
                                      actions, std::move(on_close_by_user));
}

namespace policy {

// static
base::WeakPtr<views::Widget> IdleDialogView::Show(
    Browser* browser,
    base::TimeDelta dialog_duration,
    base::TimeDelta idle_threshold,
    IdleDialog::ActionSet actions,
    base::OnceClosure on_close_by_user) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::make_unique<IdleDialogView>(dialog_duration, idle_threshold, actions,
                                       std::move(on_close_by_user)),
      browser->window()->GetNativeWindow());
  widget->Show();
  return widget->GetWeakPtr();
}

IdleDialogView::~IdleDialogView() = default;

std::u16string IdleDialogView::GetWindowTitle() const {
  int message_id;
  if (actions_.close && actions_.clear) {
    message_id = IDS_IDLE_TIMEOUT_CLOSE_AND_CLEAR_TITLE;
  } else if (actions_.close) {
    message_id = IDS_IDLE_TIMEOUT_CLOSE_TITLE;
  } else {
    CHECK(actions_.clear);
    message_id = IDS_IDLE_TIMEOUT_CLEAR_TITLE;
  }
  return l10n_util::GetStringUTF16(message_id);
}

ui::ImageModel IdleDialogView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

IdleDialogView::IdleDialogView(base::TimeDelta dialog_duration,
                               base::TimeDelta idle_threshold,
                               IdleDialog::ActionSet actions,
                               base::OnceClosure on_close_by_user)
    : idle_threshold_(idle_threshold),
      actions_(actions),
      deadline_(base::TimeTicks::Now() + dialog_duration) {
  CHECK(actions.close || actions.clear);
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_IDLE_DISMISS_BUTTON));
  SetShowIcon(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  auto [callback1, callback2] =
      base::SplitOnceCallback(std::move(on_close_by_user));
  SetAcceptCallback(std::move(callback1));
  SetCancelCallback(std::move(callback2));

  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  auto* const layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  main_label_ = AddChildView(CreateLabel());
  incognito_label_ = AddChildView(CreateLabel());
  countdown_label_ = AddChildView(CreateLabel());

  update_timer_.Start(FROM_HERE, base::Seconds(1),
                      base::BindRepeating(&IdleDialogView::UpdateCountdown,
                                          base::Unretained(this)));

  // TODO(nicolaso): In 90%+ of cases, GetIncognitoBrowserCount() is correct.
  // But sometimes, it reports the wrong number. There can be profiles that
  // _aren't_ closing, but have Incognito browsers.
  incognito_count_ = BrowserList::GetIncognitoBrowserCount();

  int main_message_id;
  if (actions_.close && actions_.clear) {
    main_message_id = IDS_IDLE_TIMEOUT_CLOSE_AND_CLEAR_BODY;
  } else if (actions_.close) {
    main_message_id = IDS_IDLE_TIMEOUT_CLOSE_BODY;
  } else {
    CHECK(actions_.clear);
    main_message_id = IDS_IDLE_TIMEOUT_CLEAR_BODY;
  }
  main_label_->SetText(l10n_util::GetPluralStringFUTF16(
      main_message_id, idle_threshold_.InMinutes()));

  if (actions_.close && incognito_count_ > 0) {
    incognito_label_->SetText(l10n_util::GetPluralStringFUTF16(
        IDS_IDLE_TIMEOUT_INCOGNITO, incognito_count_));
    incognito_label_->SetVisible(true);
  } else {
    incognito_label_->SetText(std::u16string());
    incognito_label_->SetVisible(false);
  }

  UpdateCountdown();
}

void IdleDialogView::UpdateCountdown() {
  base::TimeDelta delay =
      std::max(base::TimeDelta(), deadline_ - base::TimeTicks::Now());

  int countdown_message_id = actions_.close ? IDS_IDLE_TIMEOUT_CLOSE_COUNTDOWN
                                            : IDS_IDLE_TIMEOUT_CLEAR_COUNTDOWN;
  countdown_label_->SetText(l10n_util::GetPluralStringFUTF16(
      countdown_message_id,
      std::max(static_cast<int64_t>(0), delay.InSeconds())));
}

}  // namespace policy
