// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/policy/idle_dialog_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
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
    base::TimeDelta dialog_duration,
    base::TimeDelta idle_threshold,
    base::RepeatingClosure on_close_by_user) {
  views::Widget* widget = policy::IdleDialogView::Show(
      dialog_duration, idle_threshold, on_close_by_user);
  return widget->GetWeakPtr();
}

namespace policy {

// static
views::Widget* IdleDialogView::Show(base::TimeDelta dialog_duration,
                                    base::TimeDelta idle_threshold,
                                    base::RepeatingClosure on_close_by_user) {
  auto* view =
      new IdleDialogView(dialog_duration, idle_threshold, on_close_by_user);
  auto* widget = CreateDialogWidget(view, nullptr, nullptr);
  widget->Show();
  view->InvalidateLayout();
  return widget;
}

IdleDialogView::~IdleDialogView() = default;

std::u16string IdleDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_IDLE_TIMEOUT_TITLE);
}

ui::ImageModel IdleDialogView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

IdleDialogView::IdleDialogView(base::TimeDelta dialog_duration,
                               base::TimeDelta idle_threshold,
                               base::RepeatingClosure on_close_by_user)
    : deadline_(base::Time::Now() + dialog_duration),
      minutes_(idle_threshold.InMinutes()) {
  SetDefaultButton(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_IDLE_DISMISS_BUTTON));
  SetShowIcon(true);
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetAcceptCallback(base::BindOnce(on_close_by_user));
  SetCancelCallback(base::BindOnce(on_close_by_user));

  set_draggable(true);
  SetModalType(ui::MODAL_TYPE_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  auto* const layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  main_label_ = AddChildView(CreateLabel());
  incognito_label_ = AddChildView(CreateLabel());
  countdown_label_ = AddChildView(CreateLabel());

  update_timer_.Start(
      FROM_HERE, base::Seconds(1),
      base::BindRepeating(&IdleDialogView::UpdateBody, base::Unretained(this)));

  // TODO(nicolaso): In 90%+ of cases, GetIncognitoBrowserCount() is correct.
  // But sometimes, it reports the wrong number. There can be profiles that
  // _aren't_ closing, but have Incognito browsers.
  incognito_count_ = BrowserList::GetIncognitoBrowserCount();

  UpdateBody();
}

void IdleDialogView::UpdateBody() {
  base::TimeDelta delay = deadline_ - base::Time::Now();
  main_label_->SetText(
      l10n_util::GetPluralStringFUTF16(IDS_IDLE_TIMEOUT_BODY, minutes_));

  if (incognito_count_ > 0) {
    incognito_label_->SetText(l10n_util::GetPluralStringFUTF16(
        IDS_IDLE_TIMEOUT_INCOGNITO, incognito_count_));
    incognito_label_->SetVisible(true);
  } else {
    incognito_label_->SetText(std::u16string());
    incognito_label_->SetVisible(false);
  }

  countdown_label_->SetText(l10n_util::GetPluralStringFUTF16(
      IDS_IDLE_TIMEOUT_COUNTDOWN,
      std::max(static_cast<int64_t>(0), delay.InSeconds())));
}

}  // namespace policy
