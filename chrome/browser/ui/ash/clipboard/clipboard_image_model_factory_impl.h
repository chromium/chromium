// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_IMAGE_MODEL_FACTORY_IMPL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_IMAGE_MODEL_FACTORY_IMPL_H_

#include <list>
#include <memory>
#include <string>

#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/ash/clipboard/clipboard_image_model_request.h"

// Implements the singleton ClipboardImageModelFactory.
class ClipboardImageModelFactoryImpl : public ash::ClipboardImageModelFactory {
 public:
  ClipboardImageModelFactoryImpl();
  ClipboardImageModelFactoryImpl(ClipboardImageModelFactoryImpl&) = delete;
  ClipboardImageModelFactoryImpl& operator=(ClipboardImageModelFactoryImpl&) =
      delete;
  ~ClipboardImageModelFactoryImpl() override;

 private:
  // ash::ClipboardImageModelFactory:
  void Render(const base::UnguessableToken& id,
              const std::string& html_markup,
              const gfx::Size& bounding_box_size,
              ImageModelCallback callback) override;
  void CancelRequest(const base::UnguessableToken& id) override;
  void Activate() override;
  void Deactivate() override;
  void RenderCurrentPendingRequests() override;
  void OnShutdown() override;

  // Starts the first request in |pending_list_|.
  void StartNextRequest();

  // Called when |request_| has been idle for 2 minutes, to clean up resources.
  void OnRequestIdle();

  // Whether ClipboardImageModelFactoryImpl is activated. If not, requests are
  // queued until Activate().
  bool active_ = false;

  // Whether ClipboardImageModelFactoryImpl will render all requests until the
  // |pending_list_| is empty. When true, all requests will be rendered
  // regardless of |active_|.
  bool active_until_empty_ = false;

  // Requests which are waiting to be run.
  std::list<ClipboardImageModelRequest::Params> pending_list_;

  // The active request. Expensive to keep in memory and expensive to create,
  // deleted OnRequestIdle.
  std::unique_ptr<ClipboardImageModelRequest> request_;

  // Timer used to clean up |request_| if it is not used for 2 minutes.
  base::DelayTimer idle_timer_;

  base::WeakPtrFactory<ClipboardImageModelFactoryImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_IMAGE_MODEL_FACTORY_IMPL_H_
