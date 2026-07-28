// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_model.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/notebooks/internal/notebooks_model_observer.h"
#include "components/notebooks/public/notebook.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

namespace {

class TestNotebooksModelObserver : public NotebooksModelObserver {
 public:
  void OnNotebooksModelLoaded() override { loaded_called_ = true; }

  void OnNotebookAdded(const Notebook& notebook) override {
    last_added_id_ = notebook.id();
  }

  void OnNotebookUpdated(const Notebook& notebook) override {
    last_updated_id_ = notebook.id();
  }

  void OnNotebookRemoved(const NotebookId& id) override {
    last_removed_id_ = id;
  }

  bool loaded_called() const { return loaded_called_; }
  const std::optional<NotebookId>& last_added_id() const {
    return last_added_id_;
  }
  const std::optional<NotebookId>& last_updated_id() const {
    return last_updated_id_;
  }
  const std::optional<NotebookId>& last_removed_id() const {
    return last_removed_id_;
  }

 private:
  bool loaded_called_ = false;
  std::optional<NotebookId> last_added_id_;
  std::optional<NotebookId> last_updated_id_;
  std::optional<NotebookId> last_removed_id_;
};

TEST(NotebooksModelTest, InitialStateNotLoaded) {
  NotebooksModel model;

  EXPECT_FALSE(model.is_loaded());
}

TEST(NotebooksModelTest, SetLoadedUpdatesState) {
  NotebooksModel model;

  model.SetLoaded();

  EXPECT_TRUE(model.is_loaded());
}

TEST(NotebooksModelTest, SetLoadedNotifiesObserver) {
  NotebooksModel model;
  TestNotebooksModelObserver observer;
  model.AddObserver(&observer);

  model.SetLoaded();

  EXPECT_TRUE(observer.loaded_called());
}

TEST(NotebooksModelTest, AddNotebookStoresNotebook) {
  NotebooksModel model;
  NotebookId id(base::Uuid::GenerateRandomV4());
  Notebook notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                    base::Time::FromSecondsSinceUnixEpoch(1000));

  model.AddNotebook(notebook);

  EXPECT_TRUE(model.Contains(id));
}

TEST(NotebooksModelTest, AddNotebookNotifiesObserver) {
  NotebooksModel model;
  TestNotebooksModelObserver observer;
  model.AddObserver(&observer);
  NotebookId id(base::Uuid::GenerateRandomV4());
  Notebook notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                    base::Time::FromSecondsSinceUnixEpoch(1000));

  model.AddNotebook(notebook);

  EXPECT_EQ(observer.last_added_id(), id);
}

TEST(NotebooksModelTest, GetNotebookReturnsValue) {
  NotebooksModel model;
  NotebookId id(base::Uuid::GenerateRandomV4());
  Notebook notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                    base::Time::FromSecondsSinceUnixEpoch(1000));

  model.AddNotebook(notebook);

  std::optional<Notebook> retrieved = model.GetNotebook(id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id(), id);
}

TEST(NotebooksModelTest, GetNotebookReturnsNulloptForMissing) {
  NotebooksModel model;

  EXPECT_EQ(model.GetNotebook(NotebookId(base::Uuid::GenerateRandomV4())),
            std::nullopt);
}

TEST(NotebooksModelTest, GetAllNotebooksReturnsAll) {
  NotebooksModel model;
  Notebook notebook1(NotebookId(base::Uuid::GenerateRandomV4()),
                     base::Time::FromSecondsSinceUnixEpoch(1000),
                     base::Time::FromSecondsSinceUnixEpoch(1000));
  Notebook notebook2(NotebookId(base::Uuid::GenerateRandomV4()),
                     base::Time::FromSecondsSinceUnixEpoch(1000),
                     base::Time::FromSecondsSinceUnixEpoch(1000));
  model.AddNotebook(notebook1);
  model.AddNotebook(notebook2);

  EXPECT_THAT(model.GetAllNotebooks(),
              testing::UnorderedElementsAre(notebook1, notebook2));
}

TEST(NotebooksModelTest, UpdateNotebookUpdatesTime) {
  NotebooksModel model;
  NotebookId id(base::Uuid::GenerateRandomV4());
  base::Time initial_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  Notebook notebook(id, initial_time, initial_time);
  model.AddNotebook(notebook);

  base::Time updated_time = initial_time + base::Minutes(5);
  notebook.SetUpdateTime(updated_time);
  model.UpdateNotebook(notebook);

  EXPECT_EQ(model.GetNotebook(id)->update_time(), updated_time);
}

TEST(NotebooksModelTest, UpdateNotebookNotifiesObserver) {
  NotebooksModel model;
  TestNotebooksModelObserver observer;
  model.AddObserver(&observer);
  NotebookId id(base::Uuid::GenerateRandomV4());
  Notebook notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                    base::Time::FromSecondsSinceUnixEpoch(1000));
  model.AddNotebook(notebook);

  notebook.SetUpdateTime(base::Time::FromSecondsSinceUnixEpoch(1000) +
                         base::Minutes(5));
  model.UpdateNotebook(notebook);

  EXPECT_EQ(observer.last_updated_id(), id);
}

TEST(NotebooksModelTest, RemoveNotebookErasesNotebook) {
  NotebooksModel model;
  NotebookId id(base::Uuid::GenerateRandomV4());
  model.AddNotebook(Notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                             base::Time::FromSecondsSinceUnixEpoch(1000)));

  model.RemoveNotebook(id);

  EXPECT_FALSE(model.Contains(id));
}

TEST(NotebooksModelTest, RemoveNotebookNotifiesObserver) {
  NotebooksModel model;
  TestNotebooksModelObserver observer;
  model.AddObserver(&observer);
  NotebookId id(base::Uuid::GenerateRandomV4());
  model.AddNotebook(Notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                             base::Time::FromSecondsSinceUnixEpoch(1000)));

  model.RemoveNotebook(id);

  EXPECT_EQ(observer.last_removed_id(), id);
}

class ReentrantObserver : public NotebooksModelObserver {
 public:
  explicit ReentrantObserver(NotebooksModel* model) : model_(model) {}

  void OnNotebookAdded(const Notebook& notebook) override {
    model_->AddNotebook(Notebook(NotebookId(base::Uuid::GenerateRandomV4()),
                                 base::Time::FromSecondsSinceUnixEpoch(1000),
                                 base::Time::FromSecondsSinceUnixEpoch(1000)));
  }

 private:
  raw_ptr<NotebooksModel> model_;
};

TEST(NotebooksModelTest, MutatingDuringNotificationDies) {
  NotebooksModel model;
  ReentrantObserver observer(&model);
  model.AddObserver(&observer);

  EXPECT_CHECK_DEATH(
      model.AddNotebook(Notebook(NotebookId(base::Uuid::GenerateRandomV4()),
                                 base::Time::FromSecondsSinceUnixEpoch(1000),
                                 base::Time::FromSecondsSinceUnixEpoch(1000))));
}

TEST(NotebooksModelTest, AddNotebookDuplicateIdDies) {
  NotebooksModel model;
  NotebookId id(base::Uuid::GenerateRandomV4());
  model.AddNotebook(Notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                             base::Time::FromSecondsSinceUnixEpoch(1000)));

  EXPECT_CHECK_DEATH(model.AddNotebook(
      Notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
               base::Time::FromSecondsSinceUnixEpoch(1000))));
}

}  // namespace

}  // namespace notebooks
