// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/browsing_data/content/local_storage_helper.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread_restrictions.h"
#include "components/browsing_data/content/browsing_data_helper_browsertest.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace browsing_data {
namespace {

const char kOrigin1[] = "http://www.chromium.org";
const char kOrigin2[] = "http://www.google.com";
// This is only here to test that state for non-web-storage schemes is not
// listed by the helper. Web storage schemes are http, https, file, ftp, ws,
// and wss.
const char kOrigin3[] = "chrome://settings";

bool PutTestData(blink::mojom::StorageArea* area) {
  base::RunLoop run_loop;
  bool success = false;
  area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, std::nullopt, "source",
            base::BindLambdaForTesting([&](bool success_in) {
              run_loop.Quit();
              success = success_in;
            }));
  run_loop.Run();
  return success;
}

class LocalStorageHelperTest : public content::ContentBrowserTest {
 protected:
  storage::mojom::LocalStorageControl* GetLocalStorageControl() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetLocalStorageControl();
  }

  void CreateLocalStorageDataForTest() {
    for (const char* origin_str : {kOrigin1, kOrigin2, kOrigin3}) {
      mojo::Remote<blink::mojom::StorageArea> area;
      blink::StorageKey storage_key =
          blink::StorageKey::CreateFromStringForTesting(origin_str);
      ASSERT_FALSE(storage_key.origin().opaque());
      GetLocalStorageControl()->BindStorageArea(
          storage_key, area.BindNewPipeAndPassReceiver());
      ASSERT_TRUE(PutTestData(area.get()));
    }
  }
};

// This class is notified by LocalStorageHelper on the UI thread
// once it finishes fetching the local storage data.
class StopTestOnCallback {
 public:
  StopTestOnCallback(LocalStorageHelper* local_storage_helper,
                     base::OnceClosure quit_closure)
      : local_storage_helper_(local_storage_helper),
        quit_closure_(std::move(quit_closure)) {
    DCHECK(local_storage_helper_);
  }

  void Callback(
      const std::list<content::StorageUsageInfo>& local_storage_info) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // There's no guarantee on the order, ensure each of the two http origins
    // are there exactly once.
    ASSERT_EQ(2u, local_storage_info.size());
    bool origin1_found = false, origin2_found = false;
    for (const auto& info : local_storage_info) {
      if (info.storage_key.origin().Serialize() == kOrigin1) {
        EXPECT_FALSE(origin1_found);
        origin1_found = true;
      } else {
        ASSERT_EQ(info.storage_key.origin().Serialize(), kOrigin2);
        EXPECT_FALSE(origin2_found);
        origin2_found = true;
      }
    }
    EXPECT_TRUE(origin1_found);
    EXPECT_TRUE(origin2_found);
    std::move(quit_closure_).Run();
  }

 private:
  raw_ptr<LocalStorageHelper> local_storage_helper_;
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CallbackCompletes) {
  base::RunLoop loop;
  auto local_storage_helper = base::MakeRefCounted<LocalStorageHelper>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetStoragePartition());
  CreateLocalStorageDataForTest();
  StopTestOnCallback stop_test_on_callback(local_storage_helper.get(),
                                           loop.QuitWhenIdleClosure());
  local_storage_helper->StartFetching(base::BindOnce(
      &StopTestOnCallback::Callback, base::Unretained(&stop_test_on_callback)));
  // Blocks until StopTestOnCallback::Callback is notified.
  loop.Run();
}

}  // namespace
}  // namespace browsing_data
