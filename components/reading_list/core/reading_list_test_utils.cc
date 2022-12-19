// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_test_utils.h"

#include "base/run_loop.h"
#include "components/reading_list/core/reading_list_model.h"

ReadingListLoadObserver::ReadingListLoadObserver(ReadingListModel* model)
    : model_(model) {
  model_->AddObserver(this);
}
ReadingListLoadObserver::~ReadingListLoadObserver() {
  model_->RemoveObserver(this);
}

void ReadingListLoadObserver::Wait() {
  if (model_->loaded()) {
    return;
  }
  run_loop_.Run();
}

void ReadingListLoadObserver::ReadingListModelLoaded(
    const ReadingListModel* model) {
  run_loop_.Quit();
}
