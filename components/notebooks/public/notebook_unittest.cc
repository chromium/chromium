// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/public/notebook.h"

#include "base/time/time.h"
#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

namespace {

TEST(NotebookTest, StoresId) {
  NotebookId id(base::Uuid::GenerateRandomV4());
  Notebook notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                    base::Time::FromSecondsSinceUnixEpoch(1000));

  EXPECT_EQ(notebook.id(), id);
}

TEST(NotebookTest, StoresCreationTime) {
  base::Time creation_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  Notebook notebook(NotebookId(base::Uuid::GenerateRandomV4()), creation_time,
                    creation_time);

  EXPECT_EQ(notebook.creation_time(), creation_time);
}

TEST(NotebookTest, StoresUpdateTime) {
  base::Time update_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  Notebook notebook(NotebookId(base::Uuid::GenerateRandomV4()), update_time,
                    update_time);

  EXPECT_EQ(notebook.update_time(), update_time);
}

TEST(NotebookTest, SetUpdateTimeMutatesTime) {
  base::Time initial_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  Notebook notebook(NotebookId(base::Uuid::GenerateRandomV4()), initial_time,
                    initial_time);
  base::Time updated_time = initial_time + base::Minutes(5);

  notebook.SetUpdateTime(updated_time);

  EXPECT_EQ(notebook.update_time(), updated_time);
}

TEST(NotebookTest, EqualityOperatorReturnsTrueForEqualNotebooks) {
  NotebookId id(base::Uuid::GenerateRandomV4());
  base::Time creation_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  base::Time update_time = base::Time::FromSecondsSinceUnixEpoch(1000);

  Notebook notebook1(id, creation_time, update_time);
  Notebook notebook2(id, creation_time, update_time);

  EXPECT_EQ(notebook1, notebook2);
}

TEST(NotebookTest, EqualityOperatorReturnsFalseForDifferentId) {
  base::Time creation_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  base::Time update_time = base::Time::FromSecondsSinceUnixEpoch(1000);

  Notebook notebook1(NotebookId(base::Uuid::GenerateRandomV4()), creation_time,
                     update_time);
  Notebook notebook2(NotebookId(base::Uuid::GenerateRandomV4()), creation_time,
                     update_time);

  EXPECT_NE(notebook1, notebook2);
}

TEST(NotebookTest, EqualityOperatorReturnsFalseForDifferentCreationTime) {
  NotebookId id(base::Uuid::GenerateRandomV4());
  base::Time now = base::Time::FromSecondsSinceUnixEpoch(1000);

  Notebook notebook1(id, now, now);
  Notebook notebook2(id, now + base::Minutes(1), now);

  EXPECT_NE(notebook1, notebook2);
}

TEST(NotebookTest, EqualityOperatorReturnsFalseForDifferentUpdateTime) {
  NotebookId id(base::Uuid::GenerateRandomV4());
  base::Time now = base::Time::FromSecondsSinceUnixEpoch(1000);

  Notebook notebook1(id, now, now);
  Notebook notebook2(id, now, now + base::Minutes(1));

  EXPECT_NE(notebook1, notebook2);
}

}  // namespace

}  // namespace notebooks
