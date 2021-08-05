// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/undo_manager.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "components/undo/undo_manager_observer.h"
#include "components/undo/undo_operation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<UndoOperation*> ConvertToRawPtrVector(
    const std::vector<std::unique_ptr<UndoOperation>>& args) {
  std::vector<UndoOperation*> args_rawptrs;
  for (auto i = args.begin(); i != args.end(); ++i)
    args_rawptrs.push_back(i->get());
  return args_rawptrs;
}

}  // namespace

// UndoManagerTestApi ----------------------------------------------------------

class UndoManagerTestApi {
 public:
  // Returns all UndoOperations that are awaiting Undo or Redo.
  static std::vector<UndoOperation*> GetAllUndoOperations(
      const UndoManager& undo_manager);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UndoManagerTestApi);
};

std::vector<UndoOperation*> UndoManagerTestApi::GetAllUndoOperations(
    const UndoManager& undo_manager) {
  std::vector<UndoOperation*> result;
  for (size_t i = 0; i < undo_manager.undo_actions_.size(); ++i) {
    const std::vector<UndoOperation*> operations =
        ConvertToRawPtrVector(undo_manager.undo_actions_[i]->undo_operations());
    result.insert(result.end(), operations.begin(), operations.end());
  }
  for (size_t i = 0; i < undo_manager.redo_actions_.size(); ++i) {
    const std::vector<UndoOperation*> operations =
        ConvertToRawPtrVector(undo_manager.redo_actions_[i]->undo_operations());
    result.insert(result.end(), operations.begin(), operations.end());
  }
  // Ensure that if an Undo is in progress the UndoOperations part of that
  // UndoGroup are included in the returned set. This will ensure that any
  // changes (such as renumbering) will be applied to any potentially
  // unprocessed UndoOperations.
  if (undo_manager.undo_in_progress_action_) {
    const std::vector<UndoOperation*> operations = ConvertToRawPtrVector(
        undo_manager.undo_in_progress_action_->undo_operations());
    result.insert(result.end(), operations.begin(), operations.end());
  }

  return result;
}

namespace {

class TestUndoOperation;

// TestUndoService -------------------------------------------------------------

class TestUndoService {
 public:
  TestUndoService();
  ~TestUndoService();

  void Redo();
  void TriggerOperation();
  void RecordUndoCall();

  UndoManager undo_manager_;

  bool performing_redo_;

  int undo_operation_count_;
  int redo_operation_count_;
};

// TestUndoOperation -----------------------------------------------------------

class TestUndoOperation : public UndoOperation {
 public:
  explicit TestUndoOperation(TestUndoService* undo_service);
  ~TestUndoOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  TestUndoService* undo_service_;

  DISALLOW_COPY_AND_ASSIGN(TestUndoOperation);
};

TestUndoOperation::TestUndoOperation(TestUndoService* undo_service)
      : undo_service_(undo_service) {
}

TestUndoOperation::~TestUndoOperation() {
}

void TestUndoOperation::Undo() {
  undo_service_->TriggerOperation();
  undo_service_->RecordUndoCall();
}

int TestUndoOperation::GetUndoLabelId() const {
  return 0;
}

int TestUndoOperation::GetRedoLabelId() const {
  return 0;
}

// TestUndoService -------------------------------------------------------------

TestUndoService::TestUndoService() : performing_redo_(false),
                                     undo_operation_count_(0),
                                     redo_operation_count_(0) {
}

TestUndoService::~TestUndoService() {
}

void TestUndoService::Redo() {
  base::AutoReset<bool> incoming_changes(&performing_redo_, true);
  undo_manager_.Redo();
}

void TestUndoService::TriggerOperation() {
  undo_manager_.AddUndoOperation(std::make_unique<TestUndoOperation>(this));
}

void TestUndoService::RecordUndoCall() {
  if (performing_redo_)
    ++redo_operation_count_;
  else
    ++undo_operation_count_;
}

// TestObserver ----------------------------------------------------------------

class TestObserver : public UndoManagerObserver {
 public:
  TestObserver() : state_change_count_(0) {}
  // Returns the number of state change callbacks
  int state_change_count() { return state_change_count_; }

  void OnUndoManagerStateChange() override { ++state_change_count_; }

 private:
  int state_change_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

// Tests -----------------------------------------------------------------------

TEST(UndoServiceTest, AddUndoActions) {
  TestUndoService undo_service;

  undo_service.TriggerOperation();
  undo_service.TriggerOperation();
  EXPECT_EQ(2U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(0U, undo_service.undo_manager_.redo_count());
}

TEST(UndoServiceTest, UndoMultipleActions) {
  TestUndoService undo_service;

  undo_service.TriggerOperation();
  undo_service.TriggerOperation();

  undo_service.undo_manager_.Undo();
  EXPECT_EQ(1U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(1U, undo_service.undo_manager_.redo_count());

  undo_service.undo_manager_.Undo();
  EXPECT_EQ(0U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(2U, undo_service.undo_manager_.redo_count());

  EXPECT_EQ(2, undo_service.undo_operation_count_);
  EXPECT_EQ(0, undo_service.redo_operation_count_);
}

TEST(UndoServiceTest, RedoAction) {
  TestUndoService undo_service;

  undo_service.TriggerOperation();

  undo_service.undo_manager_.Undo();
  EXPECT_EQ(0U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(1U, undo_service.undo_manager_.redo_count());

  undo_service.Redo();
  EXPECT_EQ(1U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(0U, undo_service.undo_manager_.redo_count());

  EXPECT_EQ(1, undo_service.undo_operation_count_);
  EXPECT_EQ(1, undo_service.redo_operation_count_);
}

TEST(UndoServiceTest, GroupActions) {
  TestUndoService undo_service;

  // Add two operations in a single action.
  undo_service.undo_manager_.StartGroupingActions();
  undo_service.TriggerOperation();
  undo_service.TriggerOperation();
  undo_service.undo_manager_.EndGroupingActions();

  // Check that only one action is created.
  EXPECT_EQ(1U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(0U, undo_service.undo_manager_.redo_count());

  undo_service.undo_manager_.Undo();
  EXPECT_EQ(0U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(1U, undo_service.undo_manager_.redo_count());

  undo_service.Redo();
  EXPECT_EQ(1U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(0U, undo_service.undo_manager_.redo_count());

  // Check that both operations were called in Undo and Redo.
  EXPECT_EQ(2, undo_service.undo_operation_count_);
  EXPECT_EQ(2, undo_service.redo_operation_count_);
}

TEST(UndoServiceTest, SuspendUndoTracking) {
  TestUndoService undo_service;

  undo_service.undo_manager_.SuspendUndoTracking();
  EXPECT_TRUE(undo_service.undo_manager_.IsUndoTrakingSuspended());

  undo_service.TriggerOperation();

  undo_service.undo_manager_.ResumeUndoTracking();
  EXPECT_FALSE(undo_service.undo_manager_.IsUndoTrakingSuspended());

  EXPECT_EQ(0U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(0U, undo_service.undo_manager_.redo_count());
}

TEST(UndoServiceTest, RedoEmptyAfterNewAction) {
  TestUndoService undo_service;

  undo_service.TriggerOperation();
  undo_service.undo_manager_.Undo();
  EXPECT_EQ(0U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(1U, undo_service.undo_manager_.redo_count());

  undo_service.TriggerOperation();
  EXPECT_EQ(1U, undo_service.undo_manager_.undo_count());
  EXPECT_EQ(0U, undo_service.undo_manager_.redo_count());
}

TEST(UndoServiceTest, GetAllUndoOperations) {
  TestUndoService undo_service;

  undo_service.TriggerOperation();

  undo_service.undo_manager_.StartGroupingActions();
  undo_service.TriggerOperation();
  undo_service.TriggerOperation();
  undo_service.undo_manager_.EndGroupingActions();

  undo_service.TriggerOperation();

  undo_service.undo_manager_.Undo();
  ASSERT_EQ(2U, undo_service.undo_manager_.undo_count());
  ASSERT_EQ(1U, undo_service.undo_manager_.redo_count());

  std::vector<UndoOperation*> all_operations =
      UndoManagerTestApi::GetAllUndoOperations(undo_service.undo_manager_);
  EXPECT_EQ(4U, all_operations.size());
}

TEST(UndoServiceTest, ObserverCallbacks) {
  TestObserver observer;
  TestUndoService undo_service;
  undo_service.undo_manager_.AddObserver(&observer);
  EXPECT_EQ(0, observer.state_change_count());

  undo_service.TriggerOperation();
  EXPECT_EQ(1, observer.state_change_count());

  undo_service.undo_manager_.StartGroupingActions();
  undo_service.TriggerOperation();
  undo_service.TriggerOperation();
  undo_service.undo_manager_.EndGroupingActions();
  EXPECT_EQ(2, observer.state_change_count());

  // There should be at least 1 observer callback for undo.
  undo_service.undo_manager_.Undo();
  int callback_count_after_undo = observer.state_change_count();
  EXPECT_GT(callback_count_after_undo, 2);

  // There should be at least 1 observer callback for redo.
  undo_service.undo_manager_.Redo();
  int callback_count_after_redo = observer.state_change_count();
  EXPECT_GT(callback_count_after_redo, callback_count_after_undo);

  undo_service.undo_manager_.RemoveObserver(&observer);
  undo_service.undo_manager_.Undo();
  EXPECT_EQ(callback_count_after_redo, observer.state_change_count());
}

} // namespace
