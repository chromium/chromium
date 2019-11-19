// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/file_remover_test_util.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

void SaveBoolValueCallback(base::OnceClosure run_loop_closure,
                           bool* result_storage,
                           bool result) {
  *result_storage = result;
  std::move(run_loop_closure).Run();
}

void VerifyRemoveNow(const base::FilePath& path,
                     FileRemoverAPI* remover,
                     bool expected_result) {
  bool returned_result = !expected_result;
  base::RunLoop run_loop;
  remover->RemoveNow(
      path, base::BindOnce(&SaveBoolValueCallback, run_loop.QuitClosure(),
                           &returned_result));
  run_loop.Run();
  EXPECT_EQ(expected_result, returned_result) << path;
}

void VerifyRegisterPostRebootRemoval(const base::FilePath& path,
                                     FileRemoverAPI* remover,
                                     bool expected_result) {
  bool returned_result = !expected_result;
  base::RunLoop run_loop;
  remover->RegisterPostRebootRemoval(
      path, base::BindOnce(&SaveBoolValueCallback, run_loop.QuitClosure(),
                           &returned_result));
  run_loop.Run();
  EXPECT_EQ(expected_result, returned_result) << path;
}

}  // namespace

void VerifyRemoveNowSuccess(const base::FilePath& path,
                            FileRemoverAPI* remover) {
  VerifyRemoveNow(path, remover, /*expected_result=*/true);
}

void VerifyRemoveNowFailure(const base::FilePath& path,
                            FileRemoverAPI* remover) {
  VerifyRemoveNow(path, remover, /*expected_result=*/false);
}

void VerifyRegisterPostRebootRemovalSuccess(const base::FilePath& path,
                                            FileRemoverAPI* remover) {
  VerifyRegisterPostRebootRemoval(path, remover, /*expected_result=*/true);
}

void VerifyRegisterPostRebootRemovalFailure(const base::FilePath& path,
                                            FileRemoverAPI* remover) {
  VerifyRegisterPostRebootRemoval(path, remover, /*expected_result=*/false);
}

}  // namespace chrome_cleaner
