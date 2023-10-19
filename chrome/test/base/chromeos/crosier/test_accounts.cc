// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/test_accounts.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/values.h"

namespace crosier {

namespace {

std::unique_ptr<base::Value> ReadJsonFile(const base::FilePath& json_path) {
  int error_code = 0;
  std::string error_str;
  JSONFileValueDeserializer deserializer(json_path);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_str);
  CHECK(error_code == 0) << "Error reading json file at " << json_path
                         << ". Error code: " << error_code << " " << error_str;
  CHECK(value);
  return value;
}

}  // namespace

void GetGaiaTestAccount(std::string& out_email, std::string& out_password) {
  base::FilePath root_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);

  base::FilePath::StringPieceType kTestAccountFilePath = FILE_PATH_LITERAL(
      "chrome/browser/internal/resources/chromeos/crosier/test_accounts.json");
  base::FilePath test_accounts_path =
      base::MakeAbsoluteFilePath(root_path.Append(kTestAccountFilePath));

  std::unique_ptr<base::Value> store = ReadJsonFile(test_accounts_path);
  const base::Value::List* default_pool =
      store->GetDict().FindList("ui.gaiaPoolDefault");
  CHECK(default_pool);
  CHECK(!default_pool->empty());

  const base::Value::Dict& account =
      (*default_pool)[base::RandInt(0, default_pool->size() - 1)].GetDict();
  out_email = *account.FindString("email");
  out_password = *account.FindString("password");
}

}  // namespace crosier
