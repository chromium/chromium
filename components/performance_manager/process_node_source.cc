// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/process_node_source.h"

#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

ProcessNodeImpl* ProcessNodeSource::GetProcessNode(int render_process_id) {
  auto* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  DCHECK(render_process_host);

  auto* render_process_user_data =
      RenderProcessUserData::GetForRenderProcessHost(render_process_host);
  DCHECK(render_process_user_data);

  return render_process_user_data->process_node();
}

}  // namespace performance_manager
