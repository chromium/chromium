// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

TabModel::TabModel(std::unique_ptr<content::WebContents> contents)
    : contents_(std::move(contents)) {}

TabModel::~TabModel() = default;

void TabModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("web_contents", contents());
  dict.Add("pinned", pinned());
  dict.Add("blocked", blocked());
}
