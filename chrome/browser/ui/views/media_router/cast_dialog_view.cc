// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/md_text_button_with_down_arrow.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_access_code_cast_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_view.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom-shared.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace media_router {

CastDialogView::CastDialogView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location)
    : BubbleDialogDelegateView(anchor_view, anchor_position),
      controller_(controller),
      profile_(profile),
      metrics_(start_time, activation_location, profile) {
  DCHECK(profile);
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  sources_button_ =
      SetExtraView(std::make_unique<views::MdTextButtonWithDownArrow>(
          base::BindRepeating(&CastDialogView::ShowSourcesMenu,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_MEDIA_ROUTER_ALTERNATIVE_SOURCES_BUTTON)));
  sources_button_->SetEnabled(false);
  MaybeShowAccessCodeCastButton();
  ShowNoSinksView();
}

CastDialogView::~CastDialogView() {
  if (controller_)
    controller_->RemoveObserver(this);
}

std::u16string CastDialogView::GetWindowTitle() const {
  switch (selected_source_) {
    case SourceType::kTab:
      return dialog_title_;
    case SourceType::kDesktop:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
    default:
      NOTREACHED_NORETURN();
  }
}

void CastDialogView::OnModelUpdated(const CastDialogModel& model) {
  if (model.media_sinks().empty()) {
    scroll_position_ = 0;
    ShowNoSinksView();
  } else {
    if (scroll_view_)
      scroll_position_ = scroll_view_->GetVisibleRect().y();
    else
      ShowScrollView();
    PopulateScrollView(model.media_sinks());
    RestoreSinkListState();
    metrics_.OnSinksLoaded(base::Time::Now());
    DisableUnsupportedSinks();
  }

  // If access code casting is enabled, the sources button needs to be enabled
  // so that user can set the source before invoking the access code casting
  // flow.
  if (sources_button_)
    sources_button_->SetEnabled(!model.media_sinks().empty() ||
                                IsAccessCodeCastingEnabled());

  dialog_title_ = model.dialog_header();
  MaybeSizeToContents();
  // Update the main action button.
  DialogModelChanged();
  for (Observer& observer : observers_)
    observer.OnDialogModelUpdated(this);
}

void CastDialogView::OnControllerDestroying() {
  controller_ = nullptr;
  // We don't destroy the dialog here because if the destruction was caused by
  // activating the toolbar icon in order to close the dialog, then it would
  // cause the dialog to immediately open again.
}

void CastDialogView::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
  metrics_.OnPaint(base::Time::Now());
}

bool CastDialogView::IsCommandIdChecked(int command_id) const {
  return command_id == selected_source_;
}

bool CastDialogView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void CastDialogView::ExecuteCommand(int command_id, int event_flags) {
  SelectSource(static_cast<SourceType>(command_id));
}

void CastDialogView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastDialogView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CastDialogView::KeepShownForTesting() {
  keep_shown_for_testing_ = true;
  set_close_on_deactivate(false);
}

void CastDialogView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(gfx::Insets::TLBR(
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
      0,
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
      0));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  controller_->AddObserver(this);
  RecordSinkCountWithDelay();
}

void CastDialogView::WindowClosing() {
  for (Observer& observer : observers_)
    observer.OnDialogWillClose(this);
  metrics_.OnCloseDialog(base::Time::Now());
}

void CastDialogView::ShowAccessCodeCastDialog() {
  if (!controller_)
    return;

  CastModeSet cast_mode_set;
  switch (selected_source_) {
    case SourceType::kTab:
      cast_mode_set = {MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR};
      break;
    case SourceType::kDesktop:
      cast_mode_set = {MediaCastMode::DESKTOP_MIRROR};
      break;
    default:
      NOTREACHED_NORETURN();
  }

  AccessCodeCastDialog::Show(
      cast_mode_set, controller_->TakeMediaRouteStarter(),
      AccessCodeCastDialogOpenLocation::kBrowserCastMenu);
}

void CastDialogView::MaybeShowAccessCodeCastButton() {
  if (!IsAccessCodeCastingEnabled())
    return;

  auto callback = base::BindRepeating(&CastDialogView::ShowAccessCodeCastDialog,
                                      base::Unretained(this));

  access_code_cast_button_ = new CastDialogAccessCodeCastButton(callback);
  AddChildView(access_code_cast_button_.get());
}

void CastDialogView::ShowNoSinksView() {
  if (no_sinks_view_)
    return;
  if (scroll_view_) {
    // The dtor of |scroll_view_| removes it from the dialog.
    delete scroll_view_;
    scroll_view_ = nullptr;
    sink_views_.clear();
  }
  no_sinks_view_ = new CastDialogNoSinksView(profile_);
  AddChildView(no_sinks_view_.get());
}

void CastDialogView::ShowScrollView() {
  if (scroll_view_)
    return;
  if (no_sinks_view_) {
    // The dtor of |no_sinks_view_| removes it from the dialog.
    delete no_sinks_view_;
    no_sinks_view_ = nullptr;
  }
  scroll_view_ = new views::ScrollView();
  AddChildView(scroll_view_.get());
  constexpr int kSinkButtonHeight = 56;
  scroll_view_->ClipHeightTo(0, kSinkButtonHeight * 6.5);
}

void CastDialogView::RestoreSinkListState() {
  if (selected_sink_index_ &&
      selected_sink_index_.value() < sink_views_.size()) {
    CastDialogSinkView* sink_view =
        sink_views_.at(selected_sink_index_.value());
    // Focus on the sink so that the screen reader reads its label, which has
    // likely been updated.
    sink_view->RequestFocus();
    // If the state became AVAILABLE, the screen reader no longer needs to read
    // the label until the user selects a sink again.
    if (sink_view->sink().state == UIMediaSinkState::AVAILABLE) {
      selected_sink_index_.reset();
    }
  }

  views::ScrollBar* scroll_bar =
      const_cast<views::ScrollBar*>(scroll_view_->vertical_scroll_bar());
  if (scroll_bar) {
    scroll_view_->ScrollToPosition(scroll_bar, scroll_position_);
    scroll_view_->Layout();
  }
}

void CastDialogView::PopulateScrollView(const std::vector<UIMediaSink>& sinks) {
  sink_views_.clear();
  auto sink_list_view = std::make_unique<views::View>();
  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  // Give a little extra space below all sink views, so that focus rings may be
  // properly drawn.
  layout_manager->set_inside_border_insets(gfx::Insets::TLBR(0, 0, 4, 0));
  sink_list_view->SetLayoutManager(std::move(layout_manager));
  for (size_t i = 0; i < sinks.size(); i++) {
    auto* sink_view =
        sink_list_view->AddChildView(std::make_unique<CastDialogSinkView>(
            profile_, sinks.at(i),
            base::BindRepeating(&CastDialogView::SinkPressed,
                                base::Unretained(this), i),
            base::BindRepeating(&CastDialogView::StopPressed,
                                base::Unretained(this), i),
            base::BindRepeating(&CastDialogView::FreezePressed,
                                base::Unretained(this), i)));
    sink_views_.push_back(sink_view);
  }
  scroll_view_->SetContents(std::move(sink_list_view));

  MaybeSizeToContents();
  Layout();
}

void CastDialogView::ShowSourcesMenu() {
  if (!sources_menu_model_) {
    sources_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

    sources_menu_model_->AddCheckItemWithStringId(
        SourceType::kTab, IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE);
    sources_menu_model_->AddCheckItemWithStringId(
        SourceType::kDesktop, IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
  }

  sources_menu_runner_ = std::make_unique<views::MenuRunner>(
      sources_menu_model_.get(), views::MenuRunner::COMBOBOX);
  const gfx::Rect& screen_bounds = sources_button_->GetBoundsInScreen();
  sources_menu_runner_->RunMenuAt(
      sources_button_->GetWidget(), nullptr, screen_bounds,
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
}

void CastDialogView::SelectSource(SourceType source) {
  selected_source_ = source;
  DisableUnsupportedSinks();
  GetWidget()->UpdateWindowTitle();
  metrics_.OnCastModeSelected();
}

void CastDialogView::SinkPressed(size_t index) {
  if (!controller_)
    return;

  selected_sink_index_ = index;
  // sink() may get invalidated during CastDialogController::StartCasting()
  // due to a model update, so make a copy here.
  const UIMediaSink sink = sink_views_.at(index)->sink();
  if (sink.route) {
    metrics_.OnStopCasting(sink.route->is_local());
    // StopCasting() may trigger a model update and invalidate |sink|.
    controller_->StopCasting(sink.route->media_route_id());
  } else if (sink.issue) {
    controller_->ClearIssue(sink.issue->id());
  } else {
    absl::optional<MediaCastMode> cast_mode = GetCastModeToUse(sink);
    if (cast_mode) {
      controller_->StartCasting(sink.id, cast_mode.value());
      metrics_.OnStartCasting(base::Time::Now(), index, cast_mode.value(),
                              sink.icon_type);
    }
  }
}

void CastDialogView::StopPressed(size_t index) {
  if (!controller_) {
    return;
  }
  selected_sink_index_ = index;
  const UIMediaSink sink = sink_views_.at(index)->sink();
  if (!sink.route) {
    return;
  }
  metrics_.OnStopCasting(sink.route->is_local());
  // StopCasting() may trigger a model update and invalidate |sink|.
  controller_->StopCasting(sink.route->media_route_id());
}

void CastDialogView::FreezePressed(size_t index) {
  if (!controller_) {
    return;
  }
  selected_sink_index_ = index;
  const UIMediaSink sink = sink_views_.at(index)->sink();
  if (!sink.route) {
    return;
  }
  // If the route cannot be frozen / unfrozen, then the freeze button should
  // not be shown at all. Even so, return early just in case the callback is
  // triggered unexpectedly.
  if (!sink.freeze_info.can_freeze) {
    return;
  }
  if (sink.freeze_info.is_frozen) {
    controller_->UnfreezeRoute(sink.route->media_route_id());
  } else { /* is_frozen == false */
    controller_->FreezeRoute(sink.route->media_route_id());
  }
}

void CastDialogView::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

absl::optional<MediaCastMode> CastDialogView::GetCastModeToUse(
    const UIMediaSink& sink) const {
  // Go through cast modes in the order of preference to find one that is
  // supported and selected.
  switch (selected_source_) {
    case SourceType::kTab:
      if (base::Contains(sink.cast_modes, PRESENTATION))
        return absl::make_optional<MediaCastMode>(PRESENTATION);
      if (base::Contains(sink.cast_modes, TAB_MIRROR))
        return absl::make_optional<MediaCastMode>(TAB_MIRROR);
      break;
    case SourceType::kDesktop:
      if (base::Contains(sink.cast_modes, DESKTOP_MIRROR))
        return absl::make_optional<MediaCastMode>(DESKTOP_MIRROR);
      break;
  }
  return absl::nullopt;
}

void CastDialogView::DisableUnsupportedSinks() {
  // Go through the AVAILABLE sinks and enable or disable them depending on
  // whether they support the selected cast mode.
  for (CastDialogSinkView* sink_view : sink_views_) {
    if (sink_view->sink().state != UIMediaSinkState::AVAILABLE) {
      continue;
    }
    const bool enable = GetCastModeToUse(sink_view->sink()).has_value();
    sink_view->SetEnabledState(enable);
  }
}

void CastDialogView::RecordSinkCountWithDelay() {
  // Record the number of sinks after three seconds. This is consistent with the
  // WebUI dialog.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastDialogView::RecordSinkCount,
                     weak_factory_.GetWeakPtr()),
      MediaRouterMetrics::kDeviceCountMetricDelay);
}

void CastDialogView::RecordSinkCount() {
  metrics_.OnRecordSinkCount(sink_views_);
}

bool CastDialogView::IsAccessCodeCastingEnabled() const {
  return base::FeatureList::IsEnabled(features::kAccessCodeCastUI) &&
         GetAccessCodeCastEnabledPref(profile_);
}

BEGIN_METADATA(CastDialogView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace media_router
