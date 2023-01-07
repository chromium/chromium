// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_subject.h"

#include "components/autofill/core/browser/autofill_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using NotificationType = autofill::AutofillObserver::NotificationType;

namespace autofill {

namespace {

class TestAutofillObserver : public AutofillObserver {
 public:
  TestAutofillObserver(NotificationType notification_type,
                       bool detach_on_notify)
      : AutofillObserver(notification_type, detach_on_notify) {}

  void OnNotify() override { was_notified_ = true; }

  void Reset() { was_notified_ = false; }

  bool IsActive() { return IsInObserverList(); }

  bool was_notified() { return was_notified_; }

 private:
  bool was_notified_ = false;
};

}  // namespace

class AutofillSubjectTest : public testing::Test {
 public:
  AutofillSubjectTest() : subject_() {}

 protected:
  AutofillSubject subject_;
};

// Tests that a basic notification works.
TEST_F(AutofillSubjectTest, Simple_Notification_NoAutoDetach) {
  TestAutofillObserver observer(NotificationType::AutocompleteFormSubmitted,
                                /*detach_on_notify=*/false);

  subject_.Attach(&observer);

  EXPECT_TRUE(observer.IsActive());

  subject_.Notify(NotificationType::AutocompleteFormSubmitted);

  EXPECT_TRUE(observer.was_notified());

  // Still active.
  EXPECT_TRUE(observer.IsActive());

  subject_.Detach(&observer);

  observer.Reset();
  subject_.Notify(NotificationType::AutocompleteFormSubmitted);

  EXPECT_FALSE(observer.was_notified());
  EXPECT_FALSE(observer.IsActive());
}

// Tests that a basic notification with auto-detach works.
TEST_F(AutofillSubjectTest, Simple_Notification_WithAutoDetach) {
  TestAutofillObserver observer(NotificationType::AutocompleteFormSubmitted,
                                /*detach_on_notify=*/true);

  subject_.Attach(&observer);

  subject_.Notify(NotificationType::AutocompleteFormSubmitted);

  EXPECT_TRUE(observer.was_notified());
  EXPECT_FALSE(observer.IsActive());

  observer.Reset();
  subject_.Notify(NotificationType::AutocompleteFormSubmitted);

  EXPECT_FALSE(observer.was_notified());
  EXPECT_FALSE(observer.IsActive());
}

// Tests that NotificationType properly isolates notifications.
TEST_F(AutofillSubjectTest, MultipleObservers_SimpleNotification) {
  TestAutofillObserver submit_observer(
      NotificationType::AutocompleteFormSubmitted,
      /*detach_on_notify=*/false);
  TestAutofillObserver cleanup_observer(
      NotificationType::AutocompleteCleanupDone,
      /*detach_on_notify=*/false);

  subject_.Attach(&submit_observer);
  subject_.Attach(&cleanup_observer);

  subject_.Notify(NotificationType::AutocompleteCleanupDone);

  EXPECT_FALSE(submit_observer.was_notified());
  EXPECT_TRUE(cleanup_observer.was_notified());
}

// Tests that auto-detach doesn't detach all observers.
TEST_F(AutofillSubjectTest,
       MultipleObservers_SimpleNotification_OneAutoDetach) {
  TestAutofillObserver detach_submit_observer(
      NotificationType::AutocompleteFormSubmitted,
      /*detach_on_notify=*/true);
  TestAutofillObserver stay_submit_observer(
      NotificationType::AutocompleteFormSubmitted,
      /*detach_on_notify=*/false);

  subject_.Attach(&detach_submit_observer);
  subject_.Attach(&stay_submit_observer);

  subject_.Notify(NotificationType::AutocompleteFormSubmitted);

  EXPECT_TRUE(detach_submit_observer.was_notified());
  EXPECT_TRUE(stay_submit_observer.was_notified());

  EXPECT_FALSE(detach_submit_observer.IsActive());
  EXPECT_TRUE(stay_submit_observer.IsActive());

  detach_submit_observer.Reset();
  stay_submit_observer.Reset();

  subject_.Notify(NotificationType::AutocompleteFormSubmitted);

  EXPECT_FALSE(detach_submit_observer.was_notified());
  EXPECT_TRUE(stay_submit_observer.was_notified());

  EXPECT_FALSE(detach_submit_observer.IsActive());
  EXPECT_TRUE(stay_submit_observer.IsActive());
}

}  // namespace autofill
