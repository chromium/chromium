// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/persistent_pref_store.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/sequence_checker_impl.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

void TestCommitPendingWriteWithCallback(
    PersistentPrefStore* store,
    base::test::TaskEnvironment* task_environment) {
  base::RunLoop run_loop;
  base::SequenceCheckerImpl sequence_checker;
  store->CommitPendingWrite(base::BindOnce(
      [](base::SequenceCheckerImpl* sequence_checker, base::RunLoop* run_loop) {
        EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
        run_loop->Quit();
      },
      base::Unretained(&sequence_checker), base::Unretained(&run_loop)));
  task_environment->RunUntilIdle();
  run_loop.Run();
}
