// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/style/typography.h"

namespace gfx {
class ImageSkia;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace views {
class ImageView;
class Label;
}  // namespace views

// CredentialsItemView represents a credential view in the account chooser
// bubble.
class CredentialsItemView : public AccountAvatarFetcherDelegate,
                            public views::Button {
  METADATA_HEADER(CredentialsItemView, views::Button)

 public:
  CredentialsItemView(PressedCallback callback,
                      const std::u16string& upper_text,
                      const std::u16string& lower_text,
                      const password_manager::PasswordForm* form,
                      network::mojom::URLLoaderFactory* loader_factory,
                      const url::Origin& initiator,
                      int upper_text_style = views::style::STYLE_PRIMARY,
                      int lower_text_style = views::style::STYLE_SECONDARY);
  CredentialsItemView(const CredentialsItemView&) = delete;
  CredentialsItemView& operator=(const CredentialsItemView&) = delete;
  ~CredentialsItemView() override;

  // AccountAvatarFetcherDelegate:
  void UpdateAvatar(const gfx::ImageSkia& image) override;

  int GetPreferredHeight() const;

 private:
  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  raw_ptr<views::ImageView> image_view_;

  raw_ptr<views::Label> upper_label_ = nullptr;
  raw_ptr<views::Label> lower_label_ = nullptr;
  raw_ptr<views::ImageView> info_icon_ = nullptr;

  base::WeakPtrFactory<CredentialsItemView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
