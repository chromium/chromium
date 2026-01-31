// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_TYPES_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

enum class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiActionType {
  kNone,
  kSummary,
  kOutline,
  kSettings,
  kQA,
  kElucidation,
  kSummaryOfSelection,
};

struct COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiPageInfo {
  MahiPageInfo();
  MahiPageInfo(const MahiPageInfo&);
  MahiPageInfo(MahiPageInfo&&) noexcept;
  MahiPageInfo& operator=(const MahiPageInfo&);
  MahiPageInfo& operator=(MahiPageInfo&&) noexcept;
  ~MahiPageInfo();

  base::UnguessableToken client_id;
  base::UnguessableToken page_id;
  GURL url;
  std::u16string title;
  gfx::ImageSkia favicon_image;
  std::optional<bool> is_distillable;
  bool is_incognito = false;
};

struct COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiContextMenuRequest {
  MahiContextMenuRequest();
  MahiContextMenuRequest(const MahiContextMenuRequest&);
  MahiContextMenuRequest(MahiContextMenuRequest&&) noexcept;
  MahiContextMenuRequest& operator=(const MahiContextMenuRequest&);
  MahiContextMenuRequest& operator=(MahiContextMenuRequest&&) noexcept;
  ~MahiContextMenuRequest();

  int64_t display_id = -1;
  MahiActionType action_type = MahiActionType::kNone;
  std::optional<std::u16string> question;
  std::optional<gfx::Rect> mahi_menu_bounds;
};

struct COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiPageContent {
  MahiPageContent();
  MahiPageContent(const MahiPageContent&);
  MahiPageContent(MahiPageContent&&) noexcept;
  MahiPageContent& operator=(const MahiPageContent&);
  MahiPageContent& operator=(MahiPageContent&&) noexcept;
  ~MahiPageContent();

  base::UnguessableToken client_id;
  base::UnguessableToken page_id;
  std::u16string page_content;
};

using MahiGetContentCallback =
    base::OnceCallback<void(std::optional<MahiPageContent>)>;

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_TYPES_H_
