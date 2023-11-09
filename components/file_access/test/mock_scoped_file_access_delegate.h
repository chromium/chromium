// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_TEST_MOCK_SCOPED_FILE_ACCESS_DELEGATE_H_
#define COMPONENTS_FILE_ACCESS_TEST_MOCK_SCOPED_FILE_ACCESS_DELEGATE_H_

#include "components/file_access/scoped_file_access_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace file_access {
class MockScopedFileAccessDelegate : public ScopedFileAccessDelegate {
 public:
  MockScopedFileAccessDelegate();
  ~MockScopedFileAccessDelegate() override;
  MOCK_METHOD(void,
              RequestFilesAccess,
              (const std::vector<base::FilePath>&,
               const GURL&,
               base::OnceCallback<void(ScopedFileAccess)>),
              (override));
  MOCK_METHOD((void),
              RequestFilesAccessForSystem,
              (const std::vector<base::FilePath>&,
               base::OnceCallback<void(ScopedFileAccess)>),
              (override));
  MOCK_METHOD((void),
              RequestDefaultFilesAccess,
              (const std::vector<base::FilePath>&,
               base::OnceCallback<void(ScopedFileAccess)>),
              (override));
  MOCK_METHOD((RequestFilesAccessIOCallback),
              CreateFileAccessCallback,
              (const GURL& destination),
              (const override));
};
}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_TEST_MOCK_SCOPED_FILE_ACCESS_DELEGATE_H_
