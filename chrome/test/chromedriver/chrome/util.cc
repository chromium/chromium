// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/util.h"

#include "base/json/json_writer.h"
#include "base/values.h"

std::string SerializeValue(const base::Value* value) {
  std::string json;
  base::JSONWriter::Write(*value, &json);
  return json;
}
