// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_RECEIVER_PRESENTATION_SERVICE_DELEGATE_IMPL_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_RECEIVER_PRESENTATION_SERVICE_DELEGATE_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_observers.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class LocalPresentationManager;

// Implements the receiver side of Presentation API for local presentation
// (an offscreen presentation that is mirrored to a wireless display, or a
// presentation on a wired display).
// Created with the WebContents for a local presentation. Each
// instance is tied to a single local presentation whose ID is given during
// construction. As such, the receiver APIs are contextual with the local
// presentation. Only the main frame of the WebContents is allowed to
// make receiver Presentation API requests; requests made from any other frame
// will be rejected.
class ReceiverPresentationServiceDelegateImpl
    : public content::WebContentsUserData<
          ReceiverPresentationServiceDelegateImpl>,
      public content::ReceiverPresentationServiceDelegate {
 public:
  // Creates an instance of ReceiverPresentationServiceDelegateImpl under
  // |web_contents| and registers it as the receiver of the local
  // presentation |presentation_id| with LocalPresentationManager.
  // No-op if a ReceiverPresentationServiceDelegateImpl instance already
  // exists under |web_contents|. This class does not take ownership of
  // |web_contents|.
  static void CreateForWebContents(content::WebContents* web_contents,
                                   const std::string& presentation_id);

  ReceiverPresentationServiceDelegateImpl(
      const ReceiverPresentationServiceDelegateImpl&) = delete;
  ReceiverPresentationServiceDelegateImpl& operator=(
      const ReceiverPresentationServiceDelegateImpl&) = delete;

  // content::ReceiverPresentationServiceDelegate implementation.
  void AddObserver(
      int render_process_id,
      int render_frame_id,
      content::PresentationServiceDelegate::Observer* observer) override;
  void RemoveObserver(int render_process_id, int render_frame_id) override;
  void Reset(int render_process_id, int render_frame_id) override;
  void RegisterReceiverConnectionAvailableCallback(
      const content::ReceiverConnectionAvailableCallback&
          receiver_available_callback) override;

 private:
  friend class content::WebContentsUserData<
      ReceiverPresentationServiceDelegateImpl>;

  ReceiverPresentationServiceDelegateImpl(content::WebContents* web_contents,
                                          const std::string& presentation_id);

  const std::string presentation_id_;

  // This is an unowned pointer to the LocalPresentationManager.
  const raw_ptr<LocalPresentationManager> local_presentation_manager_;

  PresentationServiceDelegateObservers observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_RECEIVER_PRESENTATION_SERVICE_DELEGATE_IMPL_H_
