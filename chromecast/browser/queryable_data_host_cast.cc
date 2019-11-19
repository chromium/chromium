// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/queryable_data_host_cast.h"

#include "chromecast/common/mojom/queryable_data_store.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace chromecast {

QueryableDataHostCast::QueryableDataHostCast(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);
}

QueryableDataHostCast::~QueryableDataHostCast() {}

void QueryableDataHostCast::SendQueryableValue(const std::string& key,
                                               const base::Value& value) {
  for (content::RenderFrameHost* render_frame_host :
       web_contents_->GetAllFrames()) {
    mojo::Remote<shell::mojom::QueryableDataStore> queryable_data_store_remote;
    render_frame_host->GetRemoteInterfaces()->GetInterface(
        queryable_data_store_remote.BindNewPipeAndPassReceiver());
    queryable_data_store_remote->Set(key, value.Clone());
  }
}

}  // namespace chromecast
