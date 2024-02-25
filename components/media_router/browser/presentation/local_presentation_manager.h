// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_LOCAL_PRESENTATION_MANAGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_LOCAL_PRESENTATION_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/common/media_route.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace content {
class WebContents;
}

namespace media_router {
// Manages all local presentations started in the associated Profile and
// facilitates communication between the controllers and the receiver of a
// local presentation. A local presentation may be an offscreen presentation for
// Presentation API 1-UA mode, or a presentation to a wired display.
//
// Design doc:
// https://docs.google.com/document/d/1XM3jhMJTQyhEC5PDAAJFNIaKh6UUEihqZDz_ztEe4Co/edit#heading=h.hadpx5oi0gml
//
// Example usage:
//
// Receiver is created to host the local presentation and registers itself
// so that controller frames can connect to it:
//
//   LocalPresentationManager* manager =
//       LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
//           web_contents_->GetBrowserContext());
//   manager->OnLocalPresentationReceiverCreated(presentation_info,
//       base::BindRepeating(
//           &PresentationServiceImpl::OnReceiverConnectionAvailable));
//
// Controlling frame establishes connection with the receiver side, resulting
// in a connection with the two endpoints being the controller
// PresentationConnectionRemote and receiver PresentationConnectionReceiver.
// Note calling this will trigger receiver frame's
// PresentationServiceImpl::OnReceiverConnectionAvailable.
//
//   manager->RegisterLocalPresentationController(
//       presentation_info,
//       std::move(controller_connection_remote),
//       std::move(receiver_connection_receiver));
//
// Invoked on receiver's PresentationServiceImpl when controller connection is
// established.
//
//   |presentation_receiver_remote_|: mojo::Remote<T> for the implementation of
//   the blink::mojom::PresentationService interface in the renderer process.
//   |controller_connection_remote|: mojo::PendingRemote<T> for
//   blink::PresentationConnection object in controlling frame's render process.
//   |receiver_connection_receiver|: mojo::PendingReceiver<T> to be bind to
//   blink::PresentationConnection object in receiver frame's render process.
//   void PresentationServiceImpl::OnReceiverConnectionAvailable(
//       const blink::mojom::PresentationInfo& presentation_info,
//       PresentationConnectionRemote controller_connection_remote,
//       PresentationConnectionReceiver receiver_connection_receiver) {
//     presentation_receiver_remote_->OnReceiverConnectionAvailable(
//         blink::mojom::PresentationInfo::From(presentation_info),
//         std::move(controller_connection_remote),
//         std::move(receiver_connection_receiver));
//   }
//
// Send message from controlling/receiver frame to receiver/controlling frame:
//
//   |target_connection_|: member variable of mojo::PendingRemote<T> for
//                         blink::PresentationConnection type, referring to
//                         remote PresentationConnectionProxy object on
//                         receiver/controlling frame.
//   |message|: Text message to be sent.
//   PresentationConnctionPtr::SendString(
//       const blink::WebString& message) {
//     target_connection_->OnMessage(
//         blink::mojom::PresentationConnectionMessage::NewMessage(
//             message.Utf8()),
//         base::BindOnce(&OnMessageReceived));
//   }
//
// A controller or receiver leaves the local presentation (e.g., due to
// navigation) by unregistering themselves from LocalPresentation object.
//
// When the receiver is no longer associated with a local presentation, it
// shall unregister itself with LocalPresentationManager. Unregistration
// will prevent additional controllers from establishing a connection with the
// receiver:
//
//   In receiver's PSImpl::Reset() {
//     local_presentation_manager->
//         OnLocalPresentationReceiverTerminated(presentation_id);
//   }
//
// This class is not thread safe. All functions must be invoked on the UI
// thread. All callbacks passed into this class will also be invoked on UI
// thread.
class LocalPresentationManager : public KeyedService {
 public:
  // Used by
  // LocalPresentationManagerFactory::BuildServiceInstanceForBrowserContext.
  LocalPresentationManager();
  LocalPresentationManager(const LocalPresentationManager&) = delete;
  LocalPresentationManager& operator=(const LocalPresentationManager&) = delete;

  ~LocalPresentationManager() override;

  // Registers controller PresentationConnectionPtr to presentation with
  // |presentation_id| and |render_frame_id|.
  // Creates a new presentation if no presentation with |presentation_id|
  // exists.
  // |controller_connection_remote|, |receiver_connection_receiver|: Not owned
  // by this class. Ownership is transferred to presentation receiver via
  // |receiver_callback| passed below.
  virtual void RegisterLocalPresentationController(
      const blink::mojom::PresentationInfo& presentation_info,
      const content::GlobalRenderFrameHostId& render_frame_id,
      mojo::PendingRemote<blink::mojom::PresentationConnection>
          controller_connection_remote,
      mojo::PendingReceiver<blink::mojom::PresentationConnection>
          receiver_connection_receiver,
      const MediaRoute& route);

  // Unregisters controller PresentationConnectionPtr to presentation with
  // |presentation_id|, |render_frame_id|. It does nothing if there is no
  // controller that matches the provided arguments. It removes presentation
  // that matches the arguments if the presentation has no |receiver_callback|
  // and any other pending controller.
  virtual void UnregisterLocalPresentationController(
      const std::string& presentation_id,
      const content::GlobalRenderFrameHostId& render_frame_id);

  // Registers |receiver_callback| and set |receiver_web_contents| to
  // presentation with |presentation_info|.
  virtual void OnLocalPresentationReceiverCreated(
      const blink::mojom::PresentationInfo& presentation_info,
      const content::ReceiverConnectionAvailableCallback& receiver_callback,
      content::WebContents* receiver_web_contents);

  // Unregisters ReceiverConnectionAvailableCallback associated with
  // |presentation_id|.
  virtual void OnLocalPresentationReceiverTerminated(
      const std::string& presentation_id);

  // Returns true if this class has a local presentation with
  // |presentation_id|.
  virtual bool IsLocalPresentation(const std::string& presentation_id);

  // Returns true if this class has a local presentation with |web_contents|.
  virtual bool IsLocalPresentation(content::WebContents* web_contents);

  // Returns nullptr if |presentation_id| is not associated with a local
  // presentation.
  virtual const MediaRoute* GetRoute(const std::string& presentation_id);

 private:
  // Represents a local presentation registered with
  // LocalPresentationManager. Contains callback to the receiver to inform
  // it of new connections established from a controller. Contains set of
  // controllers registered to LocalPresentationManager before corresponding
  // receiver.
  class LocalPresentation {
   public:
    explicit LocalPresentation(
        const blink::mojom::PresentationInfo& presentation_info);

    LocalPresentation(const LocalPresentation&) = delete;
    LocalPresentation& operator=(const LocalPresentation&) = delete;

    ~LocalPresentation();

    // Register controller with |render_frame_id|. If |receiver_callback_| has
    // been set, invoke |receiver_callback_| with |controller_connection_remote|
    // and |receiver_connection_receiver| as parameter, else creates a
    // ControllerConnection object with |controller_connection_remote| and
    // |receiver_connection_receiver|, and store it in |pending_controllers_|
    // map.
    void RegisterController(
        const content::GlobalRenderFrameHostId& render_frame_id,
        mojo::PendingRemote<blink::mojom::PresentationConnection>
            controller_connection_remote,
        mojo::PendingReceiver<blink::mojom::PresentationConnection>
            receiver_connection_receiver,
        const MediaRoute& route);

    // Unregister controller with |render_frame_id|. Do nothing if there is no
    // pending controller with |render_frame_id|.
    void UnregisterController(
        const content::GlobalRenderFrameHostId& render_frame_id);

    // Register |receiver_callback| and set |receiver_web_contents| to current
    // local_presentation object. For each controller in |pending_controllers_|
    // map, invoke |receiver_callback| with controller as parameter. Clear
    // |pending_controllers_| map afterwards.
    void RegisterReceiver(
        const content::ReceiverConnectionAvailableCallback& receiver_callback,
        content::WebContents* receiver_web_contents);

   private:
    friend class LocalPresentationManagerTest;
    friend class LocalPresentationManager;

    // Returns false if receiver_callback_ is null and there are no pending
    // controllers.
    bool IsValid() const;

    const blink::mojom::PresentationInfo presentation_info_;
    std::optional<MediaRoute> route_;
    raw_ptr<content::WebContents, DanglingUntriaged> receiver_web_contents_ =
        nullptr;

    // Callback to invoke whenever a receiver connection is available.
    content::ReceiverConnectionAvailableCallback receiver_callback_;

    // Stores controller information.
    // |controller_connection_remote|: mojo::PendingRemote<T> to
    // blink::PresentationConnection object in controlling frame;
    // |receiver_connection_receiver|: mojo::PendingReceiver<T> to be bind to
    // blink::PresentationConnection object in receiver frame.
    struct ControllerConnection {
     public:
      ControllerConnection(
          mojo::PendingRemote<blink::mojom::PresentationConnection>
              controller_connection_remote,
          mojo::PendingReceiver<blink::mojom::PresentationConnection>
              receiver_connection_receiver);
      ~ControllerConnection();

      mojo::PendingRemote<blink::mojom::PresentationConnection>
          controller_connection_remote;
      mojo::PendingReceiver<blink::mojom::PresentationConnection>
          receiver_connection_receiver;
    };

    // Contains ControllerConnection objects registered via
    // |RegisterController()| before |receiver_callback_| is set.
    std::unordered_map<content::GlobalRenderFrameHostId,
                       std::unique_ptr<ControllerConnection>,
                       content::GlobalRenderFrameHostIdHasher>
        pending_controllers_;
  };

 private:
  friend class LocalPresentationManagerTest;
  friend class MockLocalPresentationManager;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           ConnectToLocalPresentation);

  using LocalPresentationMap =
      std::map<std::string, std::unique_ptr<LocalPresentation>>;

  // Creates a local presentation with |presentation_info|.
  LocalPresentation* GetOrCreateLocalPresentation(
      const blink::mojom::PresentationInfo& presentation_info);

  // Maps from presentation ID to LocalPresentation.
  LocalPresentationMap local_presentations_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_LOCAL_PRESENTATION_MANAGER_H_
