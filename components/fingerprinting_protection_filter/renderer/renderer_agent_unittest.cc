// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/fingerprinting_protection_filter/renderer/mock_renderer_agent.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

using subresource_filter::mojom::ActivationLevel;
using ::testing::_;
using ::testing::Return;

subresource_filter::mojom::ActivationState GetDisabledState() {
  subresource_filter::mojom::ActivationState activation_state;
  activation_state.activation_level = ActivationLevel::kDisabled;
  return activation_state;
}

subresource_filter::mojom::ActivationState GetEnabledState() {
  subresource_filter::mojom::ActivationState activation_state;
  activation_state.activation_level = ActivationLevel::kEnabled;
  return activation_state;
}

// A test class that takes the place of a real `RendererURLLoaderThrottle` and
// only implements the necessary endpoints to communicate with a
// `RendererAgent`.
class FakeURLLoaderThrottle {
 public:
  FakeURLLoaderThrottle() = default;
  ~FakeURLLoaderThrottle() = default;

  void OnActivationComputed(
      subresource_filter::mojom::ActivationState activation_state,
      RendererAgent::OnSubresourceEvaluatedCallback
          on_subresource_evaluated_callback,
      const GURL& current_document_url) {
    subresource_callback_ = std::move(on_subresource_evaluated_callback);
    activation_state_ = activation_state;
  }

  RendererAgent::ActivationComputedCallback GetActivationComputedCallback() {
    // Safe to use unretained because this object will live on the same
    // sequence as the agent and is for testing only.
    return base::BindRepeating(&FakeURLLoaderThrottle::OnActivationComputed,
                               base::Unretained(this));
  }

  const std::optional<subresource_filter::mojom::ActivationState>&
  GetActivationState() const {
    return activation_state_;
  }

  void RunSubresourceEvaluatedCallback(
      bool subresource_disallowed,
      const subresource_filter::mojom::DocumentLoadStatistics& statistics) {
    std::move(subresource_callback_)
        .Run(GURL("https://example.com"), "devtools_id", subresource_disallowed,
             statistics);
  }

 private:
  std::optional<subresource_filter::mojom::ActivationState> activation_state_;
  RendererAgent::OnSubresourceEvaluatedCallback subresource_callback_;
};

class FakeFingerprintingProtectionHost
    : public mojom::FingerprintingProtectionHost {
 public:
  FakeFingerprintingProtectionHost() = default;
  ~FakeFingerprintingProtectionHost() override = default;

  MOCK_METHOD0(DidDisallowFirstSubresource, void());

  void SetDocumentLoadStatistics(
      subresource_filter::mojom::DocumentLoadStatisticsPtr statistics)
      override {
    statistics_ = std::move(statistics);
  }

  const subresource_filter::mojom::DocumentLoadStatisticsPtr&
  GetDocumentLoadStatistics() {
    return statistics_;
  }

 private:
  subresource_filter::mojom::DocumentLoadStatisticsPtr statistics_;
};

class RendererAgentTest : public ::testing::Test {
 public:
  RendererAgentTest() {
    agent_.SetFingerprintingProtectionHost(
        static_cast<mojom::FingerprintingProtectionHost*>(&host_));
  }

  ~RendererAgentTest() override = default;

 protected:
  MockRendererAgent& agent() { return agent_; }

  base::test::TaskEnvironment task_environment_;
  FakeFingerprintingProtectionHost host_;
  MockRendererAgent agent_;
};

TEST_F(RendererAgentTest, ActivateForNextCommittedLoad) {
  // Set up the agent to observe a main frame with no inheritable activation.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillOnce(Return(GURL()))
      .WillRepeatedly(Return(GURL("https://example.com")));

  // There should still be no activation after initialization as the agent waits
  // for a signal from the browser.
  agent().Initialize();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());

  EXPECT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);
}

TEST_F(RendererAgentTest, DidCreateNewDocument_SavesActivation) {
  // Set up the agent to observe a main frame.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillOnce(Return(GURL()))
      .WillRepeatedly(Return(GURL("https://example.com")));

  agent().Initialize();
  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());

  // The enabled activation should be saved while
  // `activation_state_for_next_document` is reset.
  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
}

TEST_F(RendererAgentTest, DidFailProvisionalLoad_ResetsActivation) {
  // Set up the agent to observe a main frame.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl()).WillOnce(Return(GURL()));

  agent().Initialize();

  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());
  EXPECT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  agent().DidFailProvisionalLoad();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);
}

TEST_F(RendererAgentTest, ChildFrame_InheritsActivation) {
  // Set up the agent to observe a child frame.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));

  EXPECT_CALL(agent(), GetInheritedActivationState)
      .WillRepeatedly(Return(GetEnabledState()));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillRepeatedly(Return(GURL("https://example.com")));

  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  // The agent will attempt to inherit activation upon initialization.
  agent().Initialize();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());

  // Reset the activation state to disabled.
  agent().ActivateForNextCommittedLoad(GetDisabledState().Clone());
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());

  // The agent will again attempt to inherit activation when a new document is
  // created, which should override the previous state not obtained through
  // inheritance.
  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());
}

// This can happen for about:blank or chrome://.
TEST_F(RendererAgentTest,
       DidCreateNewDocument_IgnoresActivationForInvalidDocument) {
  // Set up the agent to observe a main frame.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl()).WillRepeatedly(Return(GURL()));

  agent().Initialize();
  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());

  // Since the document is not valid, `activation_state_to_inherit` won't be
  // updated and `activation_state_for_next_document` won't be reset.
  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  FakeURLLoaderThrottle throttle;
  agent().AddActivationComputedCallback(
      throttle.GetActivationComputedCallback());
  // Throttles will receive the activation state from the browser.
  EXPECT_EQ(throttle.GetActivationState(), GetEnabledState());
}

// This can happen if the main frame is about:blank or chrome://
TEST_F(RendererAgentTest,
       ChildFrame_DoesNotInheritNavigationFromInvalidParent) {
  // Set up the agent to observe a child frame.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));

  // GetInheritedActivationState returns std::nullopt when the main frame isn't
  // valid, see DidCreateNewDocument_IgnoresActivationForInvalidDocument.
  EXPECT_CALL(agent(), GetInheritedActivationState)
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl()).WillRepeatedly(Return(GURL()));

  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  // The agent won't inherit the state upon initialization.
  agent().Initialize();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  // Set the activation state to enabled from the browser.
  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());
  EXPECT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  // Since the document is not valid, `activation_state_to_inherit` won't be
  // updated and `activation_state_for_next_document` won't be reset.
  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  FakeURLLoaderThrottle throttle;
  agent().AddActivationComputedCallback(
      throttle.GetActivationComputedCallback());
  // Throttles will receive the activation state from the browser.
  EXPECT_EQ(throttle.GetActivationState(), GetEnabledState());
}

TEST_F(RendererAgentTest,
       ChildFrame_StillInheritsActivationAfterFailedProvisionalLoad) {
  // Set up the agent to observe a child frame.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));

  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(GetEnabledState()));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillRepeatedly(Return(GURL("https://example.com")));

  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  // The agent will attempt to inherit activation upon initialization.
  agent().Initialize();
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());

  // A failed provisional load should reset the next document activation state
  // but keep the current document state the same.
  agent().DidFailProvisionalLoad();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());

  // The inherited state should still be used after a new document is created.
  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());
}

TEST_F(RendererAgentTest, MainFrameWithOpener_InheritsActivation) {
  // Set up the agent to observe a main frame opened from another page.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(true));

  subresource_filter::mojom::ActivationState inherited_activation_state;
  inherited_activation_state.activation_level = ActivationLevel::kEnabled;
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(inherited_activation_state));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillRepeatedly(Return(GURL("https://example.com")));

  EXPECT_EQ(agent().activation_state_to_inherit(), std::nullopt);

  // The agent will attempt to inherit activation upon initialization.
  agent().Initialize();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());

  // Reset the activation state to disabled.
  subresource_filter::mojom::ActivationState disabled_state;
  agent().ActivateForNextCommittedLoad(disabled_state.Clone());
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());

  // The agent will again attempt to inherit activation when a new document is
  // created, which should override the previous state not obtained through
  // inheritance.
  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  EXPECT_EQ(agent().activation_state_to_inherit(), GetEnabledState());
}

TEST_F(RendererAgentTest, NotifiesThrottlesOfActivation_Sync) {
  // Set up the agent to observe a main frame with no inheritable activation.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillOnce(Return(GURL()))
      .WillRepeatedly(Return(GURL("https://example.com")));

  agent().Initialize();
  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());
  ASSERT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  agent().DidCreateNewDocument();

  // A throttle arrives after the agent has already received activation. The
  // agent should immediately notify the throttle.
  FakeURLLoaderThrottle throttle;
  agent().AddActivationComputedCallback(
      throttle.GetActivationComputedCallback());
  EXPECT_EQ(throttle.GetActivationState(), GetEnabledState());
}

TEST_F(RendererAgentTest, NotifiesThrottlesOfActivation_Async) {
  // Set up the agent to observe a main frame with no inheritable activation.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillOnce(Return(GURL()))
      .WillRepeatedly(Return(GURL("https://example.com")));

  agent().Initialize();

  // Two throttles arrive before the agent receives activation.
  FakeURLLoaderThrottle throttle, throttle2;
  agent().AddActivationComputedCallback(
      throttle.GetActivationComputedCallback());
  agent().AddActivationComputedCallback(
      throttle2.GetActivationComputedCallback());
  EXPECT_EQ(throttle.GetActivationState(), std::nullopt);
  EXPECT_EQ(throttle2.GetActivationState(), std::nullopt);

  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());
  ASSERT_EQ(agent().activation_state_for_next_document(), GetEnabledState());
  agent().DidCreateNewDocument();

  // All throttles should now be notified of activation.
  EXPECT_EQ(throttle.GetActivationState(), GetEnabledState());
  EXPECT_EQ(throttle2.GetActivationState(), GetEnabledState());
}

TEST_F(RendererAgentTest, NotificationsOnFrameReused) {
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillOnce(Return(GURL()))
      .WillRepeatedly(Return(GURL("https://example.com")));

  agent().Initialize();

  // A regular page is loaded.
  agent().ActivateForNextCommittedLoad(GetDisabledState().Clone());
  ASSERT_EQ(agent().activation_state_for_next_document(), GetDisabledState());
  agent().DidCreateNewDocument();
  ASSERT_EQ(agent().activation_state_for_next_document(), GetDisabledState());

  // The frame is going to be reused so a new activation is sent by the
  // browser.
  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());
  ASSERT_EQ(agent().activation_state_for_next_document(), GetEnabledState());

  // A new throttle is added from the previous document. It will use the old
  // state because the document hasn't been updated yet.
  FakeURLLoaderThrottle old_document_throttle;
  agent().AddActivationComputedCallback(
      old_document_throttle.GetActivationComputedCallback());
  EXPECT_EQ(old_document_throttle.GetActivationState(), GetDisabledState());

  agent().DidCreateNewDocument();
  EXPECT_EQ(agent().activation_state_for_next_document(), GetDisabledState());

  FakeURLLoaderThrottle new_document_throttle;
  agent().AddActivationComputedCallback(
      new_document_throttle.GetActivationComputedCallback());
  EXPECT_EQ(new_document_throttle.GetActivationState(), GetEnabledState());
}

TEST_F(RendererAgentTest, NotifiesRemoteHostOfSubresourcesEvaluated) {
  // Set up the agent to observe a main frame with no inheritable activation.
  EXPECT_CALL(agent(), IsTopLevelMainFrame()).WillRepeatedly(Return(true));
  EXPECT_CALL(agent(), HasValidOpener()).WillRepeatedly(Return(false));
  EXPECT_CALL(agent(), GetInheritedActivationState())
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(agent(), GetMainDocumentUrl())
      .WillOnce(Return(GURL()))
      .WillRepeatedly(Return(GURL("https://example.com")));

  agent().Initialize();

  // Two throttles arrive before the agent receives activation.
  FakeURLLoaderThrottle throttle, throttle2;
  agent().AddActivationComputedCallback(
      throttle.GetActivationComputedCallback());
  agent().AddActivationComputedCallback(
      throttle2.GetActivationComputedCallback());

  agent().ActivateForNextCommittedLoad(GetEnabledState().Clone());
  agent().DidCreateNewDocument();

  // All throttles should now be notified of activation.
  EXPECT_EQ(throttle.GetActivationState(), GetEnabledState());
  EXPECT_EQ(throttle2.GetActivationState(), GetEnabledState());

  subresource_filter::mojom::DocumentLoadStatistics individual_statistics(
      /*num_loads_total=*/2, /*num_loads_evaluated=*/2,
      /*num_loads_matching_rules=*/1, /*num_loads_disallowed=*/1,
      /*evaluation_total_wall_duration=*/
      base::TimeDelta(base::Microseconds(100)),
      /*evaluation_total_cpu_duration=*/
      base::TimeDelta(base::Microseconds(100)));

  // Simulate throttles notifying the agent of subresources evaluated.
  EXPECT_CALL(agent(), OnSubresourceDisallowed(_, _)).Times(2);
  throttle.RunSubresourceEvaluatedCallback(/*subresource_disallowed=*/true,
                                           individual_statistics);
  throttle2.RunSubresourceEvaluatedCallback(/*subresource_disallowed=*/true,
                                            individual_statistics);

  // The agent should aggregate statistics from each throttle and report these
  // to the remote host once the document load completes.
  subresource_filter::mojom::DocumentLoadStatistics aggregate_statistics(
      /*num_loads_total=*/4, /*num_loads_evaluated=*/4,
      /*num_loads_matching_rules=*/2, /*num_loads_disallowed=*/2,
      /*evaluation_total_wall_duration=*/
      base::TimeDelta(base::Microseconds(200)),
      /*evaluation_total_cpu_duration=*/
      base::TimeDelta(base::Microseconds(200)));
  EXPECT_TRUE(base::test::RunUntil([this, aggregate_statistics]() {
    return agent().aggregated_document_statistics() == aggregate_statistics;
  }));
  // We don't send statistics until the page finishes loading.
  EXPECT_TRUE(host_.GetDocumentLoadStatistics().is_null());

  agent().DidFinishLoad();
  EXPECT_TRUE(base::test::RunUntil([this, aggregate_statistics]() {
    return *host_.GetDocumentLoadStatistics() == aggregate_statistics;
  }));
}

}  // namespace fingerprinting_protection_filter
