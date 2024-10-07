// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

class AsyncDocumentSubresourceFilterTest : public ::testing::Test {
 public:
  AsyncDocumentSubresourceFilterTest() = default;

  AsyncDocumentSubresourceFilterTest(
      const AsyncDocumentSubresourceFilterTest&) = delete;
  AsyncDocumentSubresourceFilterTest& operator=(
      const AsyncDocumentSubresourceFilterTest&) = delete;

 protected:
  void SetUp() override {
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateAllowlistRuleForDocument(
        "allowlisted.subframe.com", proto::ACTIVATION_TYPE_GENERICBLOCK,
        {"example.com"}));
    rules.push_back(testing::CreateSuffixRule("disallowed.html"));

    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        blocking_task_runner_, kSafeBrowsingRulesetConfig);
  }

  void TearDown() override {
    dealer_handle_.reset(nullptr);
    RunUntilIdle();
  }

  const testing::TestRuleset& ruleset() const {
    return test_ruleset_pair_.indexed;
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    while (blocking_task_runner_->HasPendingTask()) {
      blocking_task_runner_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
  }

  void RunBlockingTasks() {
    if (blocking_task_runner_->HasPendingTask())
      blocking_task_runner_->RunPendingTasks();
  }

  VerifiedRulesetDealer::Handle* dealer_handle() {
    return dealer_handle_.get();
  }

  std::unique_ptr<VerifiedRuleset::Handle> CreateRulesetHandle() {
    return std::make_unique<VerifiedRuleset::Handle>(dealer_handle());
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  // Note: ADSF assumes a task runner is associated with the current thread.
  // Instantiate a MessageLoop on the current thread and use base::RunLoop to
  // handle the replies ADSF tasks generate.
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> blocking_task_runner_ =
      new base::TestSimpleTaskRunner;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;
};

namespace {

// TODO(csharrison): If more consumers need to test these callbacks at this
// granularity, consider moving these classes into
// async_document_subresource_filter_test_utils.
class TestCallbackReceiver {
 public:
  TestCallbackReceiver() = default;

  TestCallbackReceiver(const TestCallbackReceiver&) = delete;
  TestCallbackReceiver& operator=(const TestCallbackReceiver&) = delete;

  base::OnceClosure GetClosure() {
    return base::BindOnce(&TestCallbackReceiver::Callback,
                          base::Unretained(this));
  }
  int callback_count() const { return callback_count_; }

 private:
  void Callback() { ++callback_count_; }

  int callback_count_ = 0;
};

class LoadPolicyCallbackReceiver {
 public:
  LoadPolicyCallbackReceiver() = default;

  LoadPolicyCallbackReceiver(const LoadPolicyCallbackReceiver&) = delete;
  LoadPolicyCallbackReceiver& operator=(const LoadPolicyCallbackReceiver&) =
      delete;

  AsyncDocumentSubresourceFilter::LoadPolicyCallback GetCallback() {
    return base::BindOnce(&LoadPolicyCallbackReceiver::Callback,
                          base::Unretained(this));
  }
  void ExpectReceivedOnce(LoadPolicy load_policy) const {
    ASSERT_EQ(1, callback_count_);
    EXPECT_EQ(load_policy, last_load_policy_);
  }

 private:
  void Callback(LoadPolicy load_policy) {
    ++callback_count_;
    last_load_policy_ = load_policy;
  }

  int callback_count_ = 0;
  LoadPolicy last_load_policy_;
};

class MultiLoadPolicyCallbackReceiver {
 public:
  MultiLoadPolicyCallbackReceiver() = default;
  MultiLoadPolicyCallbackReceiver(
      const MultiLoadPolicyCallbackReceiver& other) = delete;
  MultiLoadPolicyCallbackReceiver& operator=(
      const MultiLoadPolicyCallbackReceiver& other) = delete;
  ~MultiLoadPolicyCallbackReceiver() = default;

  AsyncDocumentSubresourceFilter::MultiLoadPolicyCallback GetCallback() {
    return base::BindOnce(&MultiLoadPolicyCallbackReceiver::Callback,
                          base::Unretained(this));
  }

  int explicitly_allow_count() const { return explicitly_allow_count_; }

  int allow_count() const { return allow_count_; }

  int would_disallow_count() const { return would_disallow_count_; }

  int disallow_count() const { return disallow_count_; }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    CHECK(quit_closure);
    quit_closure_ = std::move(quit_closure);
  }

 private:
  void Callback(std::vector<LoadPolicy> policies) {
    for (const auto& load_policy : policies) {
      switch (load_policy) {
        case LoadPolicy::EXPLICITLY_ALLOW:
          explicitly_allow_count_++;
          break;
        case LoadPolicy::ALLOW:
          allow_count_++;
          break;
        case LoadPolicy::WOULD_DISALLOW:
          would_disallow_count_++;
          break;
        case LoadPolicy::DISALLOW:
          disallow_count_++;
      }
    }

    Quit();
  }

  void Quit() {
    CHECK(!quit_closure_.is_null());
    std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;

  int explicitly_allow_count_ = 0;
  int allow_count_ = 0;
  int would_disallow_count_ = 0;
  int disallow_count_ = 0;
};

}  // namespace

TEST_F(AsyncDocumentSubresourceFilterTest, ActivationStateIsReported) {
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());

  RunUntilIdle();
  mojom::ActivationState expected_state;
  expected_state.activation_level = mojom::ActivationLevel::kEnabled;
  activation_state.ExpectReceivedOnce(expected_state);
}

TEST_F(AsyncDocumentSubresourceFilterTest, DeleteFilter_NoActivationCallback) {
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());

  EXPECT_FALSE(filter->has_activation_state());
  filter.reset();
  RunUntilIdle();
  EXPECT_EQ(0, activation_state.callback_count());
}

TEST_F(AsyncDocumentSubresourceFilterTest, ActivationStateIsComputedCorrectly) {
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://allowlisted.subframe.com"), mojom::ActivationLevel::kEnabled,
      false);
  params.parent_document_origin =
      url::Origin::Create(GURL("http://example.com"));

  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());

  RunUntilIdle();

  mojom::ActivationState expected_activation_state;
  expected_activation_state.activation_level = mojom::ActivationLevel::kEnabled;
  expected_activation_state.generic_blocking_rules_disabled = true;
  activation_state.ExpectReceivedOnce(expected_activation_state);
}

TEST_F(AsyncDocumentSubresourceFilterTest, DisabledForCorruptRuleset) {
  testing::TestRuleset::CorruptByFilling(ruleset(), 0, 100, 0xFF);
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());

  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());

  RunUntilIdle();
  activation_state.ExpectReceivedOnce(mojom::ActivationState());
}

TEST_F(AsyncDocumentSubresourceFilterTest, GetLoadPolicyForSubdocument) {
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());

  LoadPolicyCallbackReceiver load_policy_1;
  LoadPolicyCallbackReceiver load_policy_2;
  filter->GetLoadPolicyForSubdocument(GURL("http://example.com/allowed.html"),
                                      load_policy_1.GetCallback());
  filter->GetLoadPolicyForSubdocument(
      GURL("http://example.com/disallowed.html"), load_policy_2.GetCallback());

  RunUntilIdle();
  load_policy_1.ExpectReceivedOnce(LoadPolicy::ALLOW);
  load_policy_2.ExpectReceivedOnce(LoadPolicy::DISALLOW);
}

TEST_F(AsyncDocumentSubresourceFilterTest, GetLoadPolicyForSubdocumentURLs) {
  const struct {
    mojom::ActivationLevel activation_level;
    std::vector<GURL> urls;
    int explicitly_allow_count;
    int allow_count;
    int would_disallow_count;
    int disallow_count;
  } kTestCases[] = {{mojom::ActivationLevel::kEnabled,
                     {},
                     0 /* explicitly_allow_count */,
                     0 /* allow_count */,
                     0 /* would_disallow_count */,
                     0 /* disallow_count */},
                    {mojom::ActivationLevel::kDryRun,
                     {},
                     0 /* explicitly_allow_count */,
                     0 /* allow_count */,
                     0 /* would_disallow_count */,
                     0 /* disallow_count */},
                    {mojom::ActivationLevel::kEnabled,
                     {GURL("http://alias1.com/allowed.html"),
                      GURL("http://alias2.com/disallowed.html")},
                     0 /* explicitly_allow_count */,
                     1 /* allow_count */,
                     0 /* would_disallow_count */,
                     1 /* disallow_count */},
                    {mojom::ActivationLevel::kDryRun,
                     {GURL("http://alias1.com/allowed.html"),
                      GURL("http://alias2.com/disallowed.html")},
                     0 /* explicitly_allow_count */,
                     1 /* allow_count */,
                     1 /* would_disallow_count */,
                     0 /* disallow_count */},
                    {mojom::ActivationLevel::kEnabled,
                     {GURL("http://example.alias1.com/allowed.html"),
                      GURL("http://example.alias2.com/allowed.html")},
                     0 /* explicitly_allow_count */,
                     2 /* allow_count */,
                     0 /* would_disallow_count */,
                     0 /* disallow_count */},
                    {mojom::ActivationLevel::kDryRun,
                     {GURL("http://example.alias1.com/allowed.html"),
                      GURL("http://example.alias2.com/allowed.html")},
                     0 /* explicitly_allow_count */,
                     2 /* allow_count */,
                     0 /* would_disallow_count */,
                     0 /* disallow_count */},
                    {mojom::ActivationLevel::kEnabled,
                     {GURL("http://example.alias1.com/disallowed.html"),
                      GURL("http://example.alias2.com/disallowed.html")},
                     0 /* explicitly_allow_count */,
                     0 /* allow_count */,
                     0 /* would_disallow_count */,
                     2 /* disallow_count */},
                    {mojom::ActivationLevel::kDryRun,
                     {GURL("http://example.alias1.com/disallowed.html"),
                      GURL("http://example.alias2.com/disallowed.html")},
                     0 /* explicitly_allow_count */,
                     0 /* allow_count */,
                     2 /* would_disallow_count */,
                     0 /* disallow_count */},
                    {mojom::ActivationLevel::kEnabled,
                     {GURL("http://test.alias1.com/disallowed.html"),
                      GURL("http://test.alias2.com/allowed.html"),
                      GURL("http://test.alias3.com/disallowed.html"),
                      GURL("http://test.alias4.com/disallowed.html")},
                     0 /* explicitly_allow_count */,
                     1 /* allow_count */,
                     0 /* would_disallow_count */,
                     3 /* disallow_count */},
                    {mojom::ActivationLevel::kDryRun,
                     {GURL("http://test.alias1.com/disallowed.html"),
                      GURL("http://test.alias2.com/allowed.html"),
                      GURL("http://test.alias3.com/disallowed.html"),
                      GURL("http://test.alias4.com/disallowed.html")},
                     0 /* explicitly_allow_count */,
                     1 /* allow_count */,
                     3 /* would_disallow_count */,
                     0 /* disallow_count */}};

  for (const auto& test : kTestCases) {
    dealer_handle()->TryOpenAndSetRulesetFile(
        ruleset().path, /*expected_checksum=*/0, base::DoNothing());
    auto ruleset_handle = CreateRulesetHandle();

    AsyncDocumentSubresourceFilter::InitializationParams params(
        GURL("http://example.com"), test.activation_level,
        false /* measure_performance */);

    testing::TestActivationStateCallbackReceiver activation_state;
    auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
        ruleset_handle.get(), std::move(params),
        activation_state.GetCallback());

    base::RunLoop run_loop;
    MultiLoadPolicyCallbackReceiver load_policy;
    load_policy.SetQuitClosure(run_loop.QuitClosure());
    filter->GetLoadPolicyForSubdocumentURLs(test.urls,
                                            load_policy.GetCallback());

    RunBlockingTasks();
    run_loop.Run();

    EXPECT_EQ(test.explicitly_allow_count,
              load_policy.explicitly_allow_count());
    EXPECT_EQ(test.allow_count, load_policy.allow_count());
    EXPECT_EQ(test.would_disallow_count, load_policy.would_disallow_count());
    EXPECT_EQ(test.disallow_count, load_policy.disallow_count());
  }
}

TEST_F(AsyncDocumentSubresourceFilterTest, FirstDisallowedLoadIsReported) {
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  TestCallbackReceiver first_disallowed_load_receiver;
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());
  filter->set_first_disallowed_load_callback(
      first_disallowed_load_receiver.GetClosure());

  LoadPolicyCallbackReceiver load_policy_1;
  filter->GetLoadPolicyForSubdocument(GURL("http://example.com/allowed.html"),
                                      load_policy_1.GetCallback());
  RunUntilIdle();
  load_policy_1.ExpectReceivedOnce(LoadPolicy::ALLOW);
  EXPECT_EQ(0, first_disallowed_load_receiver.callback_count());

  LoadPolicyCallbackReceiver load_policy_2;
  filter->GetLoadPolicyForSubdocument(
      GURL("http://example.com/disallowed.html"), load_policy_2.GetCallback());
  RunUntilIdle();
  load_policy_2.ExpectReceivedOnce(LoadPolicy::DISALLOW);
  EXPECT_EQ(0, first_disallowed_load_receiver.callback_count());

  filter->ReportDisallowedLoad();
  EXPECT_EQ(1, first_disallowed_load_receiver.callback_count());
  RunUntilIdle();
}

TEST_F(AsyncDocumentSubresourceFilterTest, UpdateActivationState) {
  // Properly initilize the ruleset and handle to use for computations.
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  // Initialize |filter| with a starting mojom::ActivationLevel of DRYRUN. This
  // value will be updated later on.
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), mojom::ActivationLevel::kDryRun, false);
  testing::TestActivationStateCallbackReceiver activation_state;
  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.GetCallback());

  // Make sure the ADSF computes its initial activation before updating it.
  RunUntilIdle();
  mojom::ActivationState dry_run_state;
  dry_run_state.activation_level = mojom::ActivationLevel::kDryRun;
  activation_state.ExpectReceivedOnce(dry_run_state);

  // Update the mojom::ActivationState before calling
  // GetLoadPolicyForSubdocument.
  mojom::ActivationState enabled_state;
  enabled_state.activation_level = mojom::ActivationLevel::kEnabled;
  filter->UpdateWithMoreAccurateState(enabled_state);

  LoadPolicyCallbackReceiver load_policy_1;
  filter->GetLoadPolicyForSubdocument(GURL("http://example.com/allowed.html"),
                                      load_policy_1.GetCallback());
  RunUntilIdle();
  load_policy_1.ExpectReceivedOnce(LoadPolicy::ALLOW);

  LoadPolicyCallbackReceiver load_policy_2;
  filter->GetLoadPolicyForSubdocument(
      GURL("http://example.com/disallowed.html"), load_policy_2.GetCallback());
  RunUntilIdle();
  load_policy_2.ExpectReceivedOnce(LoadPolicy::DISALLOW);
}

// Tests the second constructor.
TEST_F(AsyncDocumentSubresourceFilterTest,
       ActivationStateProvided_ActivationStateImmediatelyAvailable) {
  dealer_handle()->TryOpenAndSetRulesetFile(
      ruleset().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle = CreateRulesetHandle();

  mojom::ActivationState provided_state;
  provided_state.activation_level = mojom::ActivationLevel::kEnabled;

  auto filter = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), url::Origin::Create(GURL("http://example.com")),
      provided_state);

  EXPECT_TRUE(filter->has_activation_state());
  EXPECT_EQ(provided_state.activation_level,
            filter->activation_state().activation_level);

  // Ensure the activation is not overwritten.
  RunUntilIdle();
  EXPECT_TRUE(filter->has_activation_state());
  EXPECT_EQ(provided_state.activation_level,
            filter->activation_state().activation_level);
}

// Tests for ComputeActivationState:

class SubresourceFilterComputeActivationStateTest : public ::testing::Test {
 public:
  SubresourceFilterComputeActivationStateTest() = default;

  SubresourceFilterComputeActivationStateTest(
      const SubresourceFilterComputeActivationStateTest&) = delete;
  SubresourceFilterComputeActivationStateTest& operator=(
      const SubresourceFilterComputeActivationStateTest&) = delete;

 protected:
  void SetUp() override {
    constexpr int32_t kDocument = proto::ACTIVATION_TYPE_DOCUMENT;
    constexpr int32_t kGenericBlock = proto::ACTIVATION_TYPE_GENERICBLOCK;

    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateAllowlistRuleForDocument(
        "child1.com", kDocument, {"parent1.com", "parent2.com"}));
    rules.push_back(testing::CreateAllowlistRuleForDocument(
        "child2.com", kGenericBlock, {"parent1.com", "parent2.com"}));
    rules.push_back(testing::CreateAllowlistRuleForDocument(
        "child3.com", kDocument | kGenericBlock,
        {"parent1.com", "parent2.com"}));

    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair));
    ruleset_ = MemoryMappedRuleset::CreateAndInitialize(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  static mojom::ActivationState MakeState(
      bool filtering_disabled_for_document,
      bool generic_blocking_rules_disabled = false,
      mojom::ActivationLevel activation_level =
          mojom::ActivationLevel::kEnabled) {
    mojom::ActivationState activation_state;
    activation_state.activation_level = activation_level;
    activation_state.filtering_disabled_for_document =
        filtering_disabled_for_document;
    activation_state.generic_blocking_rules_disabled =
        generic_blocking_rules_disabled;
    return activation_state;
  }

  const MemoryMappedRuleset* ruleset() { return ruleset_.get(); }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
};

TEST_F(SubresourceFilterComputeActivationStateTest,
       ActivationBitsCorrectlyPropagateToChildDocument) {
  // TODO(pkalinnikov): Find a short way to express all these tests.
  const struct {
    const char* document_url;
    const char* parent_document_origin;
    mojom::ActivationState parent_activation;
    mojom::ActivationState expected_activation_state;
  } kTestCases[] = {
      {"http://example.com", "http://example.com", MakeState(false, false),
       MakeState(false, false)},
      {"http://example.com", "http://example.com", MakeState(false, true),
       MakeState(false, true)},
      {"http://example.com", "http://example.com", MakeState(true, false),
       MakeState(true)},
      {"http://example.com", "http://example.com", MakeState(true, true),
       MakeState(true, true)},

      {"http://child1.com", "http://parrrrent1.com", MakeState(false, false),
       MakeState(false, false)},
      {"http://child1.com", "http://parent1.com", MakeState(false, false),
       MakeState(true, false)},
      {"http://child1.com", "http://parent2.com", MakeState(false, false),
       MakeState(true, false)},
      {"http://child1.com", "http://parent2.com", MakeState(true, false),
       MakeState(true)},
      {"http://child1.com", "http://parent2.com", MakeState(false, true),
       MakeState(true, true)},

      {"http://child2.com", "http://parent1.com", MakeState(false, false),
       MakeState(false, true)},
      {"http://child2.com", "http://parent1.com", MakeState(false, true),
       MakeState(false, true)},
      {"http://child2.com", "http://parent1.com", MakeState(true, false),
       MakeState(true)},
      {"http://child2.com", "http://parent1.com", MakeState(true, true),
       MakeState(true, true)},

      {"http://child3.com", "http://parent1.com", MakeState(false, false),
       MakeState(true)},
      {"http://child3.com", "http://parent1.com", MakeState(false, true),
       MakeState(true, true)},
      {"http://child3.com", "http://parent1.com", MakeState(true, false),
       MakeState(true)},
      {"http://child3.com", "http://parent1.com", MakeState(true, true),
       MakeState(true, true)},
  };

  for (size_t i = 0, size = std::size(kTestCases); i != size; ++i) {
    SCOPED_TRACE(::testing::Message() << "Test number: " << i);
    const auto& test_case = kTestCases[i];

    GURL document_url(test_case.document_url);
    url::Origin parent_document_origin =
        url::Origin::Create(GURL(test_case.parent_document_origin));
    mojom::ActivationState activation_state =
        ComputeActivationState(document_url, parent_document_origin,
                               test_case.parent_activation, ruleset());
    EXPECT_TRUE(test_case.expected_activation_state.Equals(activation_state))
        << activation_state.filtering_disabled_for_document << " "
        << activation_state.generic_blocking_rules_disabled;
  }
}

}  // namespace subresource_filter
