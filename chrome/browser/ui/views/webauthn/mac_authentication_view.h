// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_MAC_AUTHENTICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_MAC_AUTHENTICATION_VIEW_H_

#include <os/availability.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/webauthn/local_authentication_token.h"
#include "crypto/scoped_lacontext.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

// MacAuthenticationView wraps an `LAAuthenticationView` such that it can be
// used in Views. It shows a biometric UI on macOS that collects Touch ID, and
// then triggers a callback.
class API_AVAILABLE(macos(12)) MacAuthenticationView : public views::View {
  METADATA_HEADER(MacAuthenticationView, views::View)

 public:
  using Callback = base::OnceCallback<void(
      std::optional<webauthn::LocalAuthenticationToken>)>;

  // The callback is called when Touch ID is complete with a boolean that
  // indicates whether the operation was successful and if successful, the
  // authenticated LAContext.
  explicit MacAuthenticationView(Callback callback,
                                 std::u16string touch_id_reason);
  ~MacAuthenticationView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnPaint(gfx::Canvas*) override;
  void VisibilityChanged(views::View* from, bool is_visible) override;

 private:
  struct ObjCStorage;

  void OnAuthenticationComplete(bool success);
  void OnTouchIDAnimationComplete(bool success);

  Callback callback_;
  std::unique_ptr<ObjCStorage> storage_;
  bool evaluation_requested_ = false;
  base::OneShotTimer touch_id_animation_timer_;
  const std::u16string touch_id_reason_;

  base::WeakPtrFactory<MacAuthenticationView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_MAC_AUTHENTICATION_VIEW_H_
