// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_share_target/target_util.h"

#include <memory>

#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mime_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

content::EvalJsResult ReadTextContent(content::WebContents* web_contents,
                                      const char* id) {
  const std::string script =
      base::StringPrintf("document.getElementById('%s').textContent", id);
  return content::EvalJs(web_contents, script);
}

}  // namespace

using TargetUtilBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TargetUtilBrowserTest, DataPipe) {
  const std::string kMessage = "Hello, world!";
  ASSERT_TRUE(embedded_test_server()->Start());
  storage::BlobStorageContext blob_context;
  const GURL share_target_url =
      embedded_test_server()->GetURL("/web_share_target/share.html");

  {
    const GURL app_url =
        embedded_test_server()->GetURL("/web_share_target/charts.html");

    web_app::InstallWebAppFromManifest(browser(), app_url);
  }

  const std::string boundary = net::GenerateMimeMultipartBoundary();

  const std::vector<std::string> names = {"notes"};
  const std::vector<std::string> values = {"share1.txt"};
  const std::vector<bool> is_value_file_uris = {true};
  const std::vector<std::string> filenames = {"share1.txt"};
  const std::vector<std::string> types = {"text/plain"};
  std::vector<mojo::PendingRemote<network::mojom::DataPipeGetter>>
      data_pipe_getters;
  {
    auto blob_data =
        std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
    blob_data->AppendData(kMessage);
    std::unique_ptr<storage::BlobDataHandle> blob_handle =
        blob_context.AddFinishedBlob(std::move(blob_data));

    auto blob = blink::mojom::SerializedBlob::New();
    blob->uuid = blob_handle->uuid();
    blob->size = blob_handle->size();
    storage::BlobImpl::Create(std::move(blob_handle),
                              blob->blob.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
    mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob->blob));
    blob_remote->AsDataPipeGetter(
        data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());

    data_pipe_getters.push_back(std::move(data_pipe_getter_remote));
  }

  NavigateParams nav_params(browser(), share_target_url,
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_params.post_data = web_share_target::ComputeMultipartBody(
      names, values, is_value_file_uris, filenames, types,
      std::move(data_pipe_getters), boundary);
  nav_params.extra_headers = base::StringPrintf(
      "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  ui_test_utils::NavigateToURL(&nav_params);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(kMessage, ReadTextContent(web_contents, "notes"));
}
