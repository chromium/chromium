// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view_utils.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/views/extensions/rich_radio_button.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/image_operations.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr int kDialogIconSize = extension_misc::EXTENSION_ICON_SMALLISH;
constexpr char kImageFetcherUmaClient[] = "Extensions";
inline constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "settings_overridden_dialog_icon_fetcher",
        R"(
            semantics {
              sender: "Image fetcher for the settings-overridden dialog"
              description:
                "Fetches a favicon for a search engine to display in the "
                "settings overridden dialog."
              trigger:
                "The user sees a dialog indicating their search settings have "
                "been overridden by an extension."
              data: "The URL of the search engine's favicon."
              destination: WEBSITE
              internal {
                contacts {
                  owners: "extensions/UI_OWNERS"
                }
              }
              user_data {
                type: NONE
              }
              last_reviewed: "2026-02-10"
            }
            policy {
              cookies_allowed: NO
              setting: "This feature cannot be disabled by settings."
              policy_exception_justification: "Not implemented."
            })");

// Creates a generic globe icon used when no bundled icon exists or before a
// remote favicon finishes loading.
ui::ImageModel CreateFallbackSearchIcon() {
  return ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                            ? vector_icons::kGlobeIcon
                                            : vector_icons::kGlobeOldIcon,
                                        ui::kColorIcon, kDialogIconSize);
}

// Scales an icon bitmap to the fixed icon dimensions expected by the dialog.
ui::ImageModel ResizeToDialogIcon(const gfx::ImageSkia& image_skia) {
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateResizedImage(
          image_skia, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kDialogIconSize, kDialogIconSize)));
}

// Produces the initial icon to display immediately when the dialog opens.
ui::ImageModel ResolveInitialIcon(
    const SettingsOverriddenDialogController::SettingOption::IconSource&
        icon_source) {
  if (const int* resource_id = std::get_if<int>(&icon_source)) {
    const gfx::ImageSkia* image_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(*resource_id);
    CHECK(image_skia && !image_skia->isNull());
    return ResizeToDialogIcon(*image_skia);
  }
  return CreateFallbackSearchIcon();
}

// Updates a dialog option's icon in place once its remote favicon arrives.
void OnOptionIconFetched(base::WeakPtr<RichRadioButton> button,
                         const gfx::Image& image,
                         const image_fetcher::RequestMetadata& metadata) {
  if (!button || image.IsEmpty()) {
    return;
  }
  button->SetImage(ResizeToDialogIcon(image.AsImageSkia()));
}

// Starts a network favicon fetch when an option specifies a remote icon URL.
void MaybeStartAsyncIconFetch(
    const SettingsOverriddenDialogController::SettingOption& option,
    RichRadioButton* button,
    image_fetcher::ImageFetcher* image_fetcher) {
  if (!image_fetcher) {
    return;
  }
  const GURL* favicon_url = std::get_if<GURL>(&option.icon_source);
  if (!favicon_url || !favicon_url->is_valid() ||
      !favicon_url->SchemeIsHTTPOrHTTPS()) {
    return;
  }

  image_fetcher::ImageFetcherParams fetch_params(kTrafficAnnotation,
                                                 kImageFetcherUmaClient);
  // Request an appropriate frame size for multi-resolution icon files; the
  // callback still resizes to the exact dialog icon size.
  fetch_params.set_frame_size(gfx::Size(kDialogIconSize, kDialogIconSize));
  image_fetcher->FetchImage(
      *favicon_url, base::BindOnce(&OnOptionIconFetched, button->GetWeakPtr()),
      fetch_params);
}

}  // namespace

void AddDialogContent(
    ui::DialogModel::Builder& builder,
    std::u16string dialog_description,
    ui::ElementIdentifier paragraph_id,
    const SettingsOverriddenDialogController::SettingOption& option1,
    ui::ElementIdentifier id1,
    base::RepeatingClosure callback1,
    const SettingsOverriddenDialogController::SettingOption& option2,
    ui::ElementIdentifier id2,
    base::RepeatingClosure callback2,
    image_fetcher::ImageFetcher* image_fetcher) {
  constexpr int kExplicitChoiceRadioGroupId = 1;

  // This dialog can have no initially-focused button, because neither radio
  // can be selected or have a focus ring, and the Save button is disabled until
  // a choice is made. This creates an interesting situation where we need to
  // make sure focus lands somewhere safe, and screen-readers announce the
  // dialog properly. This is important because this dialog is meant to be
  // non-escapable, so screen-readers need to work well.
  //
  // On Windows, we need to focus the paragraph element (which is explicitly
  // made focusable when created), or else the dialog does not grab focus when
  // it appears (ie. nothing is read, and a Tab keystroke is required to select
  // the dialog).
  //
  // On Mac, the dialog heading is picked up as the initially-focused element
  // and the dialog is properly introduced by screen readers. If we set an
  // initially-focused field, the focus appears to change within the dialog
  // immediately after it appears, which interrupts introduction of the dialog
  // by the screen reader.
#if BUILDFLAG(IS_WIN)
  const bool initially_focus_description = true;
#else
  const bool initially_focus_description = false;
#endif

  const auto* const layout_provider = views::LayoutProvider::Get();
  views::Label* paragraph_label = nullptr;
  RichRadioButton* button1 = nullptr;
  RichRadioButton* button2 = nullptr;
  auto container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              views::Builder<views::Label>()
                  .CopyAddressTo(&paragraph_label)
                  .SetProperty(views::kElementIdentifierKey, paragraph_id)
                  .SetText(dialog_description)
                  .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                  .SetTextStyle(views::style::STYLE_PRIMARY)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetFocusBehavior(initially_focus_description
                                        ? views::View::FocusBehavior::ALWAYS
                                        : views::View::FocusBehavior::NEVER),
              views::Builder<views::View>().SetPreferredSize(gfx::Size(
                  1, layout_provider->GetDistanceMetric(
                         views::DISTANCE_UNRELATED_CONTROL_VERTICAL))),
              views::Builder<views::Separator>(),
              views::Builder<RichRadioButton>(
                  std::make_unique<RichRadioButton>(
                      ResolveInitialIcon(option1.icon_source), option1.text,
                      option1.description, kExplicitChoiceRadioGroupId,
                      std::move(callback1)))
                  .CopyAddressTo(&button1)
                  .SetProperty(views::kElementIdentifierKey, id1),
              views::Builder<views::Separator>(),
              views::Builder<RichRadioButton>(
                  std::make_unique<RichRadioButton>(
                      ResolveInitialIcon(option2.icon_source), option2.text,
                      option2.description, kExplicitChoiceRadioGroupId,
                      std::move(callback2)))
                  .CopyAddressTo(&button2)
                  .SetProperty(views::kElementIdentifierKey, id2),
              views::Builder<views::Separator>())
          .Build();

  MaybeStartAsyncIconFetch(option1, button1, image_fetcher);
  MaybeStartAsyncIconFetch(option2, button2, image_fetcher);

  // This groups the RadioButtons contained within the RichRadioButtons (needed
  // since the RadioButtons don't share the same parent View).
  views::SetCascadingRadioGroupView(container.get(),
                                    views::kCascadingRadioGroupView);

  builder.AddCustomField(
      std::make_unique<views::BubbleDialogModelHost::CustomView>(
          std::move(container),
          views::BubbleDialogModelHost::FieldType::kControl, paragraph_label),
      paragraph_id);

  if (initially_focus_description) {
    builder.SetInitiallyFocusedField(paragraph_id);
  }
}

}  // namespace extensions
