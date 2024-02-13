// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_MAC_AUTHENTICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_MAC_AUTHENTICATION_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// MacAuthenticationView wraps an `LAAuthenticationView` such that it can be
// used in Views. It shows a biometric UI on macOS that collects Touch ID, and
// then triggers a callback.
class API_AVAILABLE(macos(12)) MacAuthenticationView : public views::View {
  METADATA_HEADER(MacAuthenticationView, views::View)

 public:
  explicit MacAuthenticationView(
      // This callback is called when Touch ID is complete with a boolean that
      // indicates whether the operation was successful.
      base::OnceCallback<void(bool)> callback);
  ~MacAuthenticationView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout(PassKey) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnPaint(gfx::Canvas*) override;
  void VisibilityChanged(views::View* from, bool is_visible) override;

 private:
  struct ObjCStorage;

  void OnAuthenticationComplete(bool success);

  base::OnceCallback<void(bool)> callback_;
  std::unique_ptr<ObjCStorage> storage_;
  bool evaluation_requested_ = false;

  base::WeakPtrFactory<MacAuthenticationView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_MAC_AUTHENTICATION_VIEW_H_
