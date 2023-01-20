// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_model_update_listener.h"

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char kHeadModelFilename[] = "on_device_head_test_model_index.bin";
static const char kTailModelFilename[] = "test_tail_model.tflite";
static const char kVocabFilename[] = "vocab_test.txt";

const base::FilePath GetTestDataDir() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.AppendASCII("components/test/data/omnibox");
  return file_path;
}

}  // namespace

class OnDeviceModelUpdateListenerTest : public testing::Test {
 protected:
  void SetUp() override {
    listener_ = OnDeviceModelUpdateListener::GetInstance();
    listener_->ResetListenerForTest();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    listener_->ResetListenerForTest();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<OnDeviceModelUpdateListener> listener_;
};

TEST_F(OnDeviceModelUpdateListenerTest, OnHeadModelUpdate) {
  base::FilePath dir_path = GetTestDataDir();
  listener_->OnHeadModelUpdate(dir_path);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(base::EndsWith(listener_->head_model_filename(),
                             kHeadModelFilename, base::CompareCase::SENSITIVE));
}

TEST_F(OnDeviceModelUpdateListenerTest, OnTailModelUpdate) {
  base::FilePath dir_path, model_path, vocab_path;
  dir_path = GetTestDataDir();
  model_path = dir_path.AppendASCII(kTailModelFilename);
  vocab_path = dir_path.AppendASCII(kVocabFilename);

  base::flat_set<base::FilePath> additional_files;
  additional_files.insert(vocab_path);
  listener_->OnTailModelUpdate(model_path, additional_files);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(base::EndsWith(listener_->tail_model_filepath().MaybeAsASCII(),
                             kTailModelFilename, base::CompareCase::SENSITIVE));
  ASSERT_TRUE(base::EndsWith(listener_->vocab_filepath().MaybeAsASCII(),
                             kVocabFilename, base::CompareCase::SENSITIVE));
}