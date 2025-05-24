// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_CONTROLLER_TEST_BASE_H_
#define COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_CONTROLLER_TEST_BASE_H_

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/test/task_environment.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/test/test_help_bubble.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/events/event_modifiers.h"

namespace user_education::test {

BASE_DECLARE_FEATURE(kTestIPHFocusHelpBubbleScreenReaderPromoFeature);

// Base test fixture that can be used to test a FeaturePromoController. Mocks or
// fakes all of the other systems the controller needs to talk to.
class FeaturePromoControllerTestBase : public testing::Test {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchorElementId);
  static const ui::ElementContext kAnchorElementContext;
  static constexpr char kTestFocusHelpBubbleAcceleratorPromoRead[] =
      "test_focus_help_bubble_accelerator_promo_read";

  FeaturePromoControllerTestBase();
  ~FeaturePromoControllerTestBase() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Wrapper for a promo controller that implements all the application-specific
  // methods.
  template <class T>
    requires std::derived_from<T, FeaturePromoControllerCommon>
  class TestPromoController : public T {
   public:
    using T::T;
    ~TestPromoController() override = default;

   protected:
    ui::ElementContext GetAnchorContext() const override {
      return kAnchorElementContext;
    }
    const ui::AcceleratorProvider* GetAcceleratorProvider() const override {
      return &test_accelerator_provider_;
    }
    std::u16string GetBodyIconAltText() const override {
      return u"Body Icon Alt Text";
    }
    const base::Feature* GetScreenReaderPromptPromoFeature() const override {
      return &kTestIPHFocusHelpBubbleScreenReaderPromoFeature;
    }
    const char* GetScreenReaderPromptPromoEventName() const override {
      return kTestFocusHelpBubbleAcceleratorPromoRead;
    }
    std::u16string GetTutorialScreenReaderHint() const override {
      return u"Tutorial Screen Reader Hint";
    }
    std::u16string GetFocusHelpBubbleScreenReaderHint(
        FeaturePromoSpecification::PromoType promo_type,
        ui::TrackedElement* anchor_element) const override {
      return u"Focus Help Bubble Screen Reader Hint";
    }

   private:
    // Accelerator provider that always returns F6.
    class DummyAcceleratorProvider : public ui::AcceleratorProvider {
     public:
      DummyAcceleratorProvider() = default;
      ~DummyAcceleratorProvider() override = default;
      bool GetAcceleratorForCommandId(int,
                                      ui::Accelerator* accel) const override {
        *accel = ui::Accelerator(ui::KeyboardCode::VKEY_F6, ui::MODIFIER_NONE);
        return true;
      }
    };

    DummyAcceleratorProvider test_accelerator_provider_;
  };

  // Sets the mock tracker's initialization success.
  void SetTrackerResult(bool success);

  // Finds the current help bubble.
  TestHelpBubble* GetHelpBubble(
      std::optional<ui::ElementContext> context = std::nullopt) const;

  feature_engagement::test::MockTracker& tracker() { return tracker_; }
  FeaturePromoRegistry& promo_registry() { return promo_registry_; }
  HelpBubbleFactoryRegistry& help_bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }
  UserEducationStorageService& storage_service() { return storage_service_; }
  MockFeaturePromoSessionPolicy& session_policy() { return session_policy_; }
  TutorialService& tutorial_service() { return tutorial_service_; }
  ProductMessagingController& messaging_controller() {
    return messaging_controller_;
  }
  FeaturePromoControllerCommon& promo_controller() {
    return *promo_controller_;
  }
  ui::test::TestElement& anchor_element() { return anchor_element_; }

  // Implemented by derived classes to create the controller.
  virtual std::unique_ptr<FeaturePromoControllerCommon> CreateController() = 0;

 private:
  class TestTutorialService : public TutorialService {
   public:
    using TutorialService::TutorialService;
    ~TestTutorialService() override;

   protected:
    // TutorialService:
    std::u16string GetBodyIconAltText(bool is_last_step) const override;
  };

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  feature_engagement::test::MockTracker tracker_;
  FeaturePromoRegistry promo_registry_;
  TestUserEducationStorageService storage_service_;

  // Have a default anchor element that starts visible, to make things easier.
  // Tests can create their own elements in this context or others.
  ui::test::TestElement anchor_element_{kAnchorElementId,
                                        kAnchorElementContext};

  // Don't use a completely faked tutorial service for now; it's sufficient to
  // use the help bubble factory with test help bubbles instead.
  HelpBubbleFactoryRegistry help_bubble_factory_registry_;
  TutorialRegistry tutorial_registry_;
  TestTutorialService tutorial_service_{&tutorial_registry_,
                                        &help_bubble_factory_registry_};

  // Use a vanilla product messaging controller since the messages themselves
  // can be mocked. The associated session provider will never start a new
  // session since that is not being tested.
  TestUserEducationSessionProvider session_provider_{false};
  ProductMessagingController messaging_controller_;

  MockFeaturePromoSessionPolicy session_policy_;
  std::unique_ptr<FeaturePromoControllerCommon> promo_controller_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_CONTROLLER_TEST_BASE_H_
