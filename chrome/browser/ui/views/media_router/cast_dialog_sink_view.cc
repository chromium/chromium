// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_view.h"

#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::ImageView> CreateConnectedIconView(
    media_router::UIMediaSink sink) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      *media_router::CastDialogSinkButton::GetVectorIcon(sink),
      kColorMediaRouterIconActive, media_router::kPrimaryIconSize));
  icon_view->SetBorder(
      views::CreateEmptyBorder(media_router::kPrimaryIconBorder));
  return icon_view;
}

std::unique_ptr<views::StyledLabel> CreateTitle(
    const media_router::UIMediaSink& sink) {
  auto title = std::make_unique<views::StyledLabel>();
  title->SetText(sink.friendly_name);
  title->SizeToFit(0);
  return title;
}

std::unique_ptr<views::View> CreateSubtitle(
    const media_router::UIMediaSink& sink,
    views::Button::PressedCallback issue_pressed_callback) {
  if (sink.issue) {
    auto subtitle_button = std::make_unique<views::LabelButton>(
        std::move(issue_pressed_callback), sink.GetStatusTextForDisplay());
    subtitle_button->SetLabelStyle(views::style::STYLE_SECONDARY);
    subtitle_button->GetViewAccessibility().SetName(
        sink.GetStatusTextForDisplay());
    return subtitle_button;
  }

  auto subtitle = std::make_unique<views::Label>(sink.GetStatusTextForDisplay(),
                                                 views::style::CONTEXT_BUTTON,
                                                 views::style::STYLE_SECONDARY);
  subtitle->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle->SetAutoColorReadabilityEnabled(false);
  return subtitle;
}

}  // namespace

namespace media_router {

CastDialogSinkView::CastDialogSinkView(
    Profile* profile,
    const UIMediaSink& sink,
    views::Button::PressedCallback sink_pressed_callback,
    views::Button::PressedCallback issue_pressed_callback,
    views::Button::PressedCallback stop_pressed_callback,
    views::Button::PressedCallback freeze_pressed_callback)
    : profile_(profile), sink_(sink) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // If the sink is connected, then add labels and buttons. Else, default to a
  // CastDialogSinkButton.
  if (sink.state == UIMediaSinkState::CONNECTED) {
    // When sink is connected, the sink view looks like this:
    //
    // *----------------------------------*
    // |      | Title                     |
    // | Icon |---------------------------| Label View
    // |      | Subtitle                  |
    // |----------------------------------|
    // |            | Button 1 | Button 2 | Buttons View
    // *----------------------------------*
    AddChildView(CreateLabelView(sink, std::move(issue_pressed_callback)));
    AddChildView(CreateButtonsView(std::move(stop_pressed_callback),
                                   std::move(freeze_pressed_callback)));
  } else {
    cast_sink_button_ = AddChildView(std::make_unique<CastDialogSinkButton>(
        std::move(sink_pressed_callback), sink));
  }
}

std::unique_ptr<views::View> CastDialogSinkView::CreateLabelView(
    const UIMediaSink& sink,
    views::Button::PressedCallback issue_pressed_callback) {
  // The spacing and padding needed to mimic the layout of the icon and labels
  // from the CastDialogSinkButton, which is implemented as a HoverButton.
  const int horizontal_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_CONTROL_LIST_VERTICAL) /
                               2;

  auto label_container = std::make_unique<views::View>();
  auto* manager =
      label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, horizontal_padding), icon_label_spacing));
  manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Add icon.
  label_container->AddChildView(CreateConnectedIconView(sink));

  // Create the wrapper so labels can stack.
  auto label_wrapper = std::make_unique<views::View>();
  title_ = label_wrapper->AddChildView(CreateTitle(sink));
  subtitle_ = label_wrapper->AddChildView(
      CreateSubtitle(sink, std::move(issue_pressed_callback)));

  // Set wrapper properties.
  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  label_wrapper->SetProperty(views::kBoxLayoutFlexKey,
                             views::BoxLayoutFlexSpecification());
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(vertical_spacing, 0));

  label_container->AddChildView(std::move(label_wrapper));

  return label_container;
}

std::unique_ptr<views::View> CastDialogSinkView::CreateButtonsView(
    views::Button::PressedCallback stop_pressed_callback,
    views::Button::PressedCallback freeze_pressed_callback) {
  const int button_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // Ensure the buttons stack next to each other and are aligned properly.
  auto button_container = std::make_unique<views::View>();
  auto* manager =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  manager->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  // Set spacing between the buttons, if there is more than one.
  manager->set_between_child_spacing(button_spacing);

  // Set the top margin to be a negative value that matches the vertical spacing
  // at the bottom of the label view. The vertical spacing is important in the
  // label view so the icon view is properly centered. However, we also want no
  // spacing between the label subtitle and the buttons.
  button_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(-ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_CONTROL_LIST_VERTICAL) /
                            2,
                        0, 0, button_spacing));

  // `can_freeze` should only ever be set to true if freeze ui is enabled, but
  // sanity check here anyways. Freeze button will also not show if the route is
  // not freezable.
  if (IsAccessCodeCastFreezeUiEnabled(profile_) &&
      sink_.freeze_info.can_freeze) {
    auto freeze_button = std::make_unique<views::MdTextButton>(
        std::move(freeze_pressed_callback),
        l10n_util::GetStringUTF16(sink_.freeze_info.is_frozen
                                      ? IDS_MEDIA_ROUTER_SINK_VIEW_RESUME
                                      : IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE));
    freeze_button->SetStyle(ui::ButtonStyle::kText);
    freeze_button->GetViewAccessibility().SetName(
        GetFreezeButtonAccessibleName());
    freeze_button_ = button_container->AddChildView(std::move(freeze_button));
  }

  // Always create the stop button, since at this point we know the sink is
  // connected.
  auto stop_button = std::make_unique<views::MdTextButton>(
      std::move(stop_pressed_callback),
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_STOP));
  stop_button->SetStyle(ui::ButtonStyle::kText);
  stop_button->GetViewAccessibility().SetName(GetStopButtonAccessibleName());
  stop_button_ = button_container->AddChildView(std::move(stop_button));

  return button_container;
}

void CastDialogSinkView::RequestFocus() {
  if (cast_sink_button_) {
    cast_sink_button_->RequestFocus();
  } else if (freeze_button_) {
    freeze_button_->RequestFocus();
  } else if (stop_button_) {
    stop_button_->RequestFocus();
  }
}

void CastDialogSinkView::SetEnabledState(bool enabled) {
  if (cast_sink_button_) {
    cast_sink_button_->SetEnabled(enabled);
  }
  if (stop_button_) {
    stop_button_->SetEnabled(enabled);
  }
  if (freeze_button_) {
    freeze_button_->SetEnabled(enabled);
  }
}

std::u16string CastDialogSinkView::GetFreezeButtonAccessibleName() const {
  // If there is no route for the sink or the route may not be frozen, no freeze
  // button should be displayed.
  if (!sink_.route || !sink_.freeze_info.can_freeze) {
    NOTREACHED_IN_MIGRATION();
    return std::u16string();
  }

  const MediaSource& source = sink_.route->media_source();
  int accessible_name = 0;
  if (sink_.freeze_info.is_frozen) {
    if (source.IsTabMirroringSource()) {
      accessible_name = IDS_MEDIA_ROUTER_SINK_VIEW_RESUME_TAB_ACCESSIBLE_NAME;
    } else if (source.IsDesktopMirroringSource()) {
      accessible_name =
          IDS_MEDIA_ROUTER_SINK_VIEW_RESUME_SCREEN_ACCESSIBLE_NAME;
    } else {
      accessible_name =
          IDS_MEDIA_ROUTER_SINK_VIEW_RESUME_GENERIC_ACCESSIBLE_NAME;
    }
  } else {
    if (source.IsTabMirroringSource()) {
      accessible_name = IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE_TAB_ACCESSIBLE_NAME;
    } else if (source.IsDesktopMirroringSource()) {
      accessible_name = IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE_SCREEN_ACCESSIBLE_NAME;
    } else {
      accessible_name =
          IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE_GENERIC_ACCESSIBLE_NAME;
    }
  }

  return l10n_util::GetStringFUTF16(accessible_name, sink_.friendly_name);
}

std::u16string CastDialogSinkView::GetStopButtonAccessibleName() const {
  if (!sink_.route) {
    NOTREACHED_IN_MIGRATION();
    return std::u16string();
  }

  const MediaSource& source = sink_.route->media_source();
  int accessible_name = 0;
  if (source.IsTabMirroringSource()) {
    accessible_name = IDS_MEDIA_ROUTER_SINK_VIEW_STOP_TAB_ACCESSIBLE_NAME;
  } else if (source.IsDesktopMirroringSource()) {
    accessible_name = IDS_MEDIA_ROUTER_SINK_VIEW_STOP_SCREEN_ACCESSIBLE_NAME;
  } else {
    accessible_name = IDS_MEDIA_ROUTER_SINK_VIEW_STOP_GENERIC_ACCESSIBLE_NAME;
  }

  return l10n_util::GetStringFUTF16(accessible_name, sink_.friendly_name);
}

CastDialogSinkView::~CastDialogSinkView() = default;

BEGIN_METADATA(CastDialogSinkView)
END_METADATA

}  // namespace media_router
