// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/progress_event.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_plugin_container.h"

using blink::WebString;
using blink::WebPluginContainer;

namespace nacl {

namespace {
const char* EventTypeToName(PP_NaClEventType event_type) {
  switch (event_type) {
    case PP_NACL_EVENT_LOADSTART:
      return "loadstart";
    case PP_NACL_EVENT_PROGRESS:
      return "progress";
    case PP_NACL_EVENT_ERROR:
      return "error";
    case PP_NACL_EVENT_ABORT:
      return "abort";
    case PP_NACL_EVENT_LOAD:
      return "load";
    case PP_NACL_EVENT_LOADEND:
      return "loadend";
    case PP_NACL_EVENT_CRASH:
      return "crash";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

void DispatchProgressEventOnMainThread(PP_Instance instance,
                                       const ProgressEvent& event) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance)
    return;

  WebPluginContainer* container = plugin_instance->GetContainer();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;

  container->DispatchProgressEvent(
      WebString::FromUTF8(EventTypeToName(event.event_type)),
      event.length_is_computable, event.loaded_bytes, event.total_bytes,
      WebString::FromUTF8(event.resource_url));
}

}  // namespace

void DispatchProgressEvent(PP_Instance instance, const ProgressEvent& event) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::BindOnce(&DispatchProgressEventOnMainThread, instance, event));
}

}  // namespace nacl
