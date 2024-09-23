// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_RESOURCES_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_RESOURCES_H_

#include "base/memory/raw_ptr.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class Plugin;

// PNaCl tool files / resources, which are opened by the browser.
struct PnaclResourceEntry {
  // The name of the tool that corresponds to the opened file.
  std::string tool_name;

  // File info for the executables, after they've been opened.
  // Only valid after StartLoad() has been called, and until
  // TakeFileInfo(ResourceType) is called.
  PP_NaClFileInfo file_info;
};

// Loads a list of resources, providing a way to get file descriptors for
// these resources.  URLs for resources are resolved by the manifest
// and point to PNaCl component filesystem resources.
class PnaclResources {
 public:
  PnaclResources(Plugin* plugin, bool use_subzero);

  PnaclResources(const PnaclResources&) = delete;
  PnaclResources& operator=(const PnaclResources&) = delete;

  virtual ~PnaclResources();

  // Read the resource info JSON file.  This is the first step after
  // construction; it has to be completed before StartLoad is called.
  bool ReadResourceInfo();

  // Start loading the resources.
  bool StartLoad();

  enum ResourceType { LLC, LD, SUBZERO, NUM_TYPES };

  const std::string& GetUrl(ResourceType type) const;

  PP_NaClFileInfo TakeFileInfo(ResourceType type);

 private:
  // The plugin requesting the resource loading.
  raw_ptr<Plugin> plugin_;
  bool use_subzero_;

  PnaclResourceEntry resources_[NUM_TYPES + 1];
};

}  // namespace plugin
#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_PNACL_RESOURCES_H_
