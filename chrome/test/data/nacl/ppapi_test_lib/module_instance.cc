// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements the required interfaces for representing a plugin module
// instance in browser interactions and provides a way to register custom
// plugin interfaces.
//

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <map>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/internal_utils.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"

///////////////////////////////////////////////////////////////////////////////
// Plugin interface registration
///////////////////////////////////////////////////////////////////////////////

namespace {

class PluginInterfaceTable {
 public:
  // Return singleton intsance.
  static PluginInterfaceTable* Get() {
    static PluginInterfaceTable table;
    return &table;
  }

  void AddInterface(const char* interface_name, const void* ppp_interface) {
    interface_map_[nacl::string(interface_name)] = ppp_interface;
  }
  const void* GetInterface(const char* interface_name) {
    // This will add a NULL element for missing interfaces.
    return interface_map_[nacl::string(interface_name)];
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginInterfaceTable);

  PluginInterfaceTable() {}

  typedef std::map<nacl::string, const void*> InterfaceMap;
  InterfaceMap interface_map_;
};

}  // namespace

void RegisterPluginInterface(const char* interface_name,
                             const void* ppp_interface) {
  PluginInterfaceTable::Get()->AddInterface(interface_name, ppp_interface);
}


///////////////////////////////////////////////////////////////////////////////
// PPP_Instance implementation
///////////////////////////////////////////////////////////////////////////////

PP_Bool DidCreateDefault(PP_Instance instance,
                         uint32_t /*argc*/,
                         const char* /*argn*/[],
                         const char* /*argv*/[]) {
  CHECK(ppb_get_interface() != NULL);
  CHECK(PPBCore() != NULL);
  CHECK(PPBGraphics2D() != NULL);
  CHECK(PPBImageData() != NULL);
  CHECK(PPBInstance() != NULL);
  CHECK(PPBMessaging() != NULL);
  CHECK(PPBURLLoader() != NULL);
  CHECK(PPBURLRequestInfo() != NULL);
  CHECK(PPBURLResponseInfo() != NULL);
  CHECK(PPBVar() != NULL);

  set_pp_instance(instance);
  SetupTests();

  return PP_TRUE;
}

void DidDestroyDefault(PP_Instance /*instance*/) {
}

void DidChangeViewDefault(PP_Instance /*instance*/, PP_Resource /*view*/) {
}

void DidChangeFocusDefault(PP_Instance /*instance*/,
                           PP_Bool /*has_focus*/) {
}

PP_Bool HandleDocumentLoadDefault(PP_Instance instance,
                                  PP_Resource url_loader) {
  return PP_TRUE;
}

namespace {

const PPP_Instance ppp_instance_interface = {
  DidCreateDefault,
  DidDestroyDefault,
  DidChangeViewDefault,
  DidChangeFocusDefault,
  HandleDocumentLoadDefault
};

///////////////////////////////////////////////////////////////////////////////
// PPP_Messaging implementation
///////////////////////////////////////////////////////////////////////////////

void HandleMessage(PP_Instance instance, PP_Var message) {
  if (message.type != PP_VARTYPE_STRING)
    return;
  uint32_t len = 0;
  const char* test_name = PPBVar()->VarToUtf8(message, &len);
  RunTest(test_name);
}

const PPP_Messaging ppp_messaging_interface = {
  HandleMessage
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PPP implementation
///////////////////////////////////////////////////////////////////////////////

int32_t PPP_InitializeModule(PP_Module module,
                             PPB_GetInterface get_browser_interface) {
  set_pp_module(module);
  set_ppb_get_interface(get_browser_interface);
  SetupPluginInterfaces();
  return PP_OK;
}

void PPP_ShutdownModule() {
}

const void* PPP_GetInterface(const char* interface_name) {
  const void* ppp = PluginInterfaceTable::Get()->GetInterface(interface_name);

  // The PPP_Instance interface is required for every plugin,
  // so supply one if the tester has not.
  if (ppp == NULL && 0 == strncmp(PPP_INSTANCE_INTERFACE, interface_name,
                                  strlen(PPP_INSTANCE_INTERFACE))) {
    return &ppp_instance_interface;
  }
  // The PPP_Messaging interface is required for the test set-up,
  // so we supply our own.
  if (0 == strncmp(PPP_MESSAGING_INTERFACE, interface_name,
                   strlen(PPP_MESSAGING_INTERFACE))) {
    CHECK(ppp == NULL);
    return &ppp_messaging_interface;
  }
  // All other interfaces are to be optionally supplied by the tester,
  // so we return whatever was added in SetupPluginInterfaces() (if anything).
  return ppp;
}
