// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_access_code_cast_button.h"

#include <memory>
#include <utility>

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/vector_icons.h"

namespace media_router {

namespace {

std::unique_ptr<views::ImageView> CreatePrimaryIconView() {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kKeyboardIcon, ui::kColorIcon, kPrimaryIconSize));
  icon_view->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon_view;
}

std::u16string CreateText() {
  return l10n_util::GetStringUTF16(IDS_ACCESS_CODE_CAST_CONNECT);
}

}  // namespace

CastDialogAccessCodeCastButton::CastDialogAccessCodeCastButton(
    PressedCallback callback)
    : HoverButton(std::move(callback),
                  CreatePrimaryIconView(),
                  CreateText(),
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

BEGIN_METADATA(CastDialogAccessCodeCastButton)
END_METADATA

}  // namespace media_router
