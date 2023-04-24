// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/media_router/browser/presentation/local_presentation_manager.h"
#include "components/media_router/browser/test/test_helper.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using blink::mojom::PresentationConnectionResultPtr;
using blink::mojom::PresentationInfo;
using testing::_;

namespace media_router {

namespace {
const char kPresentationId[] = "presentationId";
const char kPresentationId2[] = "presentationId2";
const char kPresentationUrl[] = "http://www.example.com/presentation.html";
}  // namespace

class MockReceiverConnectionAvailableCallback {
 public:
  MOCK_METHOD1(OnReceiverConnectionAvailable,
               void(PresentationConnectionResultPtr));
};

class LocalPresentationManagerTest : public content::RenderViewHostTestHarness {
 public:
  LocalPresentationManagerTest()
      : render_frame_host_id_(1, 1),
        presentation_info_(GURL(kPresentationUrl), kPresentationId),
        route_("route_1", MediaSource("source_1"), "sink_1", "", false) {}

  LocalPresentationManager* manager() { return &manager_; }

  void VerifyPresentationsSize(size_t expected) {
    EXPECT_EQ(expected, manager_.local_presentations_.size());
  }

  void VerifyControllerSize(size_t expected,
                            const std::string& presentationId) {
    EXPECT_TRUE(base::Contains(manager_.local_presentations_, presentationId));
    EXPECT_EQ(expected, manager_.local_presentations_[presentationId]
                            ->pending_controllers_.size());
  }

  void RegisterController(
      const std::string& presentation_id,
      mojo::PendingRemote<blink::mojom::PresentationConnection> controller) {
    RegisterController(
        PresentationInfo(GURL(kPresentationUrl), presentation_id),
        render_frame_host_id_, std::move(controller));
  }

  void RegisterController(
      const content::GlobalRenderFrameHostId& render_frame_id,
      mojo::PendingRemote<blink::mojom::PresentationConnection> controller) {
    RegisterController(presentation_info_, render_frame_id,
                       std::move(controller));
  }

  void RegisterController(
      mojo::PendingRemote<blink::mojom::PresentationConnection> controller) {
    RegisterController(presentation_info_, render_frame_host_id_,
                       std::move(controller));
  }

  void RegisterController(
      const PresentationInfo& presentation_info,
      const content::GlobalRenderFrameHostId& render_frame_id,
      mojo::PendingRemote<blink::mojom::PresentationConnection> controller) {
    mojo::PendingReceiver<blink::mojom::PresentationConnection>
        receiver_conn_receiver;
    manager()->RegisterLocalPresentationController(
        presentation_info, render_frame_id, std::move(controller),
        std::move(receiver_conn_receiver), route_);
  }

  void RegisterReceiver(
      MockReceiverConnectionAvailableCallback& receiver_callback) {
    RegisterReceiver(kPresentationId, receiver_callback);
  }

  void RegisterReceiver(
      const std::string& presentation_id,
      MockReceiverConnectionAvailableCallback& receiver_callback) {
    manager()->OnLocalPresentationReceiverCreated(
        PresentationInfo(GURL(kPresentationUrl), presentation_id),
        base::BindRepeating(&MockReceiverConnectionAvailableCallback::
                                OnReceiverConnectionAvailable,
                            base::Unretained(&receiver_callback)),
        web_contents());
  }

  void UnregisterController(
      const content::GlobalRenderFrameHostId& render_frame_id) {
    manager()->UnregisterLocalPresentationController(kPresentationId,
                                                     render_frame_id);
  }

  void UnregisterController() {
    manager()->UnregisterLocalPresentationController(kPresentationId,
                                                     render_frame_host_id_);
  }

  void UnregisterReceiver() {
    manager()->OnLocalPresentationReceiverTerminated(kPresentationId);
  }

 private:
  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const PresentationInfo presentation_info_;
  LocalPresentationManager manager_;
  MediaRoute route_;
};

TEST_F(LocalPresentationManagerTest, RegisterUnregisterController) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  RegisterController(std::move(controller));
  VerifyPresentationsSize(1);
  UnregisterController();
  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest, RegisterUnregisterReceiver) {
  MockReceiverConnectionAvailableCallback receiver_callback;
  RegisterReceiver(receiver_callback);
  VerifyPresentationsSize(1);
  UnregisterReceiver();
  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest, UnregisterNonexistentController) {
  UnregisterController();
  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest, UnregisterNonexistentReceiver) {
  UnregisterReceiver();
  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest,
       RegisterMultipleControllersSamePresentation) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(content::GlobalRenderFrameHostId(1, 1),
                     std::move(controller1));
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller2;
  RegisterController(content::GlobalRenderFrameHostId(1, 2),
                     std::move(controller2));
  VerifyPresentationsSize(1);
}

TEST_F(LocalPresentationManagerTest,
       RegisterMultipleControllersDifferentPresentations) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(kPresentationId, std::move(controller1));
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller2;
  RegisterController(kPresentationId2, std::move(controller2));
  VerifyPresentationsSize(2);
}

TEST_F(LocalPresentationManagerTest,
       RegisterControllerThenReceiverInvokesCallback) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_));
  RegisterReceiver(receiver_callback);
}

TEST_F(LocalPresentationManagerTest,
       UnregisterReceiverFromConnectedPresentation) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_));
  RegisterReceiver(receiver_callback);
  UnregisterReceiver();

  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest,
       UnregisterControllerFromConnectedPresentation) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_));
  RegisterReceiver(receiver_callback);
  UnregisterController();

  VerifyPresentationsSize(1);
}

TEST_F(LocalPresentationManagerTest,
       UnregisterReceiverThenControllerFromConnectedPresentation) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_));
  RegisterReceiver(receiver_callback);
  UnregisterReceiver();
  UnregisterController();

  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest,
       UnregisterControllerThenReceiverFromConnectedPresentation) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_));
  RegisterReceiver(receiver_callback);
  UnregisterController();
  UnregisterReceiver();

  VerifyPresentationsSize(0);
}

TEST_F(LocalPresentationManagerTest,
       RegisterTwoControllersThenReceiverInvokesCallbackTwice) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(content::GlobalRenderFrameHostId(1, 1),
                     std::move(controller1));
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller2;
  RegisterController(content::GlobalRenderFrameHostId(1, 2),
                     std::move(controller2));

  MockReceiverConnectionAvailableCallback receiver_callback;
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_)).Times(2);
  RegisterReceiver(receiver_callback);
}

TEST_F(LocalPresentationManagerTest,
       RegisterControllerReceiverConontrollerInvokesCallbackTwice) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(content::GlobalRenderFrameHostId(1, 1),
                     std::move(controller1));

  MockReceiverConnectionAvailableCallback receiver_callback;
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_)).Times(2);
  RegisterReceiver(receiver_callback);

  mojo::PendingRemote<blink::mojom::PresentationConnection> controller2;
  RegisterController(content::GlobalRenderFrameHostId(1, 2),
                     std::move(controller2));
}

TEST_F(LocalPresentationManagerTest,
       UnregisterFirstControllerFromeConnectedPresentation) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(content::GlobalRenderFrameHostId(1, 1),
                     std::move(controller1));
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller2;
  RegisterController(content::GlobalRenderFrameHostId(1, 2),
                     std::move(controller2));

  MockReceiverConnectionAvailableCallback receiver_callback;
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailable(_)).Times(2);
  RegisterReceiver(receiver_callback);
  UnregisterController(content::GlobalRenderFrameHostId(1, 1));
  UnregisterController(content::GlobalRenderFrameHostId(1, 1));

  VerifyPresentationsSize(1);
}

TEST_F(LocalPresentationManagerTest, TwoPresentations) {
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(kPresentationId, std::move(controller1));

  MockReceiverConnectionAvailableCallback receiver_callback1;
  EXPECT_CALL(receiver_callback1, OnReceiverConnectionAvailable(_)).Times(1);
  RegisterReceiver(kPresentationId, receiver_callback1);

  mojo::PendingRemote<blink::mojom::PresentationConnection> controller2;
  RegisterController(kPresentationId2, std::move(controller2));

  MockReceiverConnectionAvailableCallback receiver_callback2;
  EXPECT_CALL(receiver_callback2, OnReceiverConnectionAvailable(_)).Times(1);
  RegisterReceiver(kPresentationId2, receiver_callback2);

  VerifyPresentationsSize(2);

  UnregisterReceiver();
  VerifyPresentationsSize(1);
}

TEST_F(LocalPresentationManagerTest,
       TestIsLocalPresentationWithPresentationId) {
  EXPECT_FALSE(manager()->IsLocalPresentation(kPresentationId));
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller1;
  RegisterController(kPresentationId, std::move(controller1));
  EXPECT_TRUE(manager()->IsLocalPresentation(kPresentationId));
}

TEST_F(LocalPresentationManagerTest, TestIsLocalPresentationWithWebContents) {
  EXPECT_FALSE(manager()->IsLocalPresentation(web_contents()));
  MockReceiverConnectionAvailableCallback receiver_callback;
  RegisterReceiver(receiver_callback);
  EXPECT_TRUE(manager()->IsLocalPresentation(web_contents()));
}

TEST_F(LocalPresentationManagerTest, TestRegisterAndGetRoute) {
  MediaSource source("source_1");
  MediaRoute route("route_1", source, "sink_1", "", false);

  EXPECT_FALSE(manager()->GetRoute(kPresentationId));
  mojo::PendingRemote<blink::mojom::PresentationConnection> controller;
  RegisterController(std::move(controller));

  auto* actual_route = manager()->GetRoute(kPresentationId);
  EXPECT_TRUE(actual_route);
  EXPECT_EQ(route, *actual_route);
}

}  // namespace media_router
