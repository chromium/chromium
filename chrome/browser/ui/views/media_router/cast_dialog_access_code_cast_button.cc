// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_access_code_cast_button.h"

#include <memory>
#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/vector_icons.h"

namespace media_router {

namespace {

bool ShouldSaveDevice(PrefService* pref_service) {
  return GetAccessCodeDeviceDurationPref(pref_service) > base::Seconds(1);
}

std::unique_ptr<views::ImageView> CreatePrimaryIconView(
    const gfx::ImageSkia& image) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  icon_view->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon_view;
}

// TODO(b/202529859): Change icons based on final UX design
std::unique_ptr<views::View> CreatePrimaryIconForProfile(
    PrefService* pref_service) {
  const gfx::VectorIcon& icon =
      ShouldSaveDevice(pref_service) ? kAddIcon : kTvIcon;

  auto image =
      gfx::CreateVectorIcon(icon, kPrimaryIconSize, gfx::kChromeIconGrey);

  return CreatePrimaryIconView(image);
}

// TODO(b/202529859): Change text to match final UX design
std::u16string CreateTextForProfile(PrefService* pref_service) {
  return ShouldSaveDevice(pref_service) ? u"Add new device"
                                        : u"Cast to a new device";
}

}  // namespace

CastDialogAccessCodeCastButton::CastDialogAccessCodeCastButton(
    PressedCallback callback,
    PrefService* pref_service)
    : HoverButton(std::move(callback),
                  CreatePrimaryIconForProfile(pref_service),
                  CreateTextForProfile(pref_service),
                  /** button subtitle */ std::u16string(),
                  /** secondary_icon_view */ nullptr) {}

CastDialogAccessCodeCastButton::~CastDialogAccessCodeCastButton() = default;

bool CastDialogAccessCodeCastButton::OnMousePressed(
    const ui::MouseEvent& event) {
  if (event.IsRightMouseButton())
    return true;
  return HoverButton::OnMousePressed(event);
}

void CastDialogAccessCodeCastButton::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (event.IsRightMouseButton())
    return;
  return HoverButton::OnMouseReleased(event);
}

BEGIN_METADATA(CastDialogAccessCodeCastButton, HoverButton)
END_METADATA

}  // namespace media_router
