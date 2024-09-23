// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/scoped_test_system_nss_key_slot_mixin.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Returns a subdirectory under the user data directory (which is cleared in
// between tests, but not after pre-tests).
base::FilePath GetNssDbTestDir() {
  base::FilePath nss_db_subdir;
  base::PathService::Get(chrome::DIR_USER_DATA, &nss_db_subdir);
  CHECK(!nss_db_subdir.empty()) << "DIR_USER_DATA is not initialized yet.";
  nss_db_subdir = nss_db_subdir.AppendASCII("nss_db_subdir");
  return nss_db_subdir;
}

}  // namespace

ScopedTestSystemNSSKeySlotMixin::ScopedTestSystemNSSKeySlotMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

ScopedTestSystemNSSKeySlotMixin::~ScopedTestSystemNSSKeySlotMixin() = default;

void ScopedTestSystemNSSKeySlotMixin::SetUpInProcessBrowserTestFixture() {
  // NSS is allowed to do IO on the current thread since dispatching
  // to a dedicated thread would still have the affect of blocking
  // the current thread, due to NSS's internal locking requirements
  base::ScopedAllowBlockingForTesting allow_blocking;

  crypto::EnsureNSSInit();

  base::FilePath nss_db_subdir = GetNssDbTestDir();
  ASSERT_TRUE(base::CreateDirectory(nss_db_subdir));

  const char kTestDescription[] = "Test DB";
  slot_ = crypto::OpenSoftwareNSSDB(nss_db_subdir, kTestDescription);
  ASSERT_TRUE(!!slot_);

  if (slot_) {
    crypto::PrepareSystemSlotForTesting(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())));
  }
}

void ScopedTestSystemNSSKeySlotMixin::TearDownOnMainThread() {
  base::RunLoop loop;
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ScopedTestSystemNSSKeySlotMixin::DestroyOnIo,
                     base::Unretained(this)),
      loop.QuitClosure());
  loop.Run();
}

void ScopedTestSystemNSSKeySlotMixin::DestroyOnIo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  crypto::ResetSystemSlotForTesting();

  if (slot_) {
    SECStatus status = crypto::CloseSoftwareNSSDB(slot_.get());
    if (status != SECSuccess)
      PLOG(ERROR) << "CloseSoftwareNSSDB failed: " << PORT_GetError();
  }
}

}  // namespace ash
