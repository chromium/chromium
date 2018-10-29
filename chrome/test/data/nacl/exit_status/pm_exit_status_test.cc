/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Post-message based test for testing crash detection.
 */
#include <string>

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "base/format_macros.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

void Initialize(const pp::Var& message_data, std::string* out) {
  *out = "hello world";
}

void RunExit0(const pp::Var& message_data, std::string* out) {
  *out = "good bye cruel world";
  // the out string should not actually get sent back in reply, since
  // we exit immediately.
  exit(0);
}

void RunExit7(const pp::Var& message_data, std::string* out) {
  *out = "good bye cruel world";
  // the out string should not actually get sent back in reply, since
  // we exit immediately.
  exit(7);
}

void RunExit254(const pp::Var& message_data, std::string* out) {
  *out = "good bye cruel world";
  // the out string should not actually get sent back in reply, since
  // we exit immediately.
  exit(254);
}

void RunExitNeg2(const pp::Var& message_data, std::string* out) {
  *out = "good bye cruel world";
  // the out string should not actually get sent back in reply, since
  // we exit immediately.
  exit(-2);
}

struct PostMessageHandlerDesc {
  char const *request;
  void (*handler)(const pp::Var& message_data, std::string* out);
};

// This object represents one time the page says <embed>.
class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~MyInstance() {}
  virtual void HandleMessage(const pp::Var& message_data);
};

// HandleMessage gets invoked when postMessage is called on the DOM
// element associated with this plugin instance.  In this case, if we
// are given a string, we'll post a message back to JavaScript with a
// reply string -- essentially treating this as a string-based RPC.
void MyInstance::HandleMessage(const pp::Var& message_data) {
  static struct PostMessageHandlerDesc kMsgHandlers[] = {
    { "init", Initialize },
    { "exit0", RunExit0 },
    { "exit7", RunExit7 },
    { "exit254", RunExit254 },
    { "exitneg2", RunExitNeg2 },
    { reinterpret_cast<char const *>(NULL),
      reinterpret_cast<void (*)(const pp::Var&, std::string*)>(NULL) }
  };

  if (message_data.is_string()) {
    std::string op_name(message_data.AsString());
    size_t len;

    fprintf(stderr, "Searching for handler for request \"%s\".\n",
            op_name.c_str());

    std::string sb;

    for (size_t ix = 0; kMsgHandlers[ix].request != NULL; ++ix) {
      if (op_name == kMsgHandlers[ix].request) {
        fprintf(stderr, "found at index %" PRIuS "\n", ix);
        kMsgHandlers[ix].handler(message_data, &sb);
        break;
      }
    }

    len = strlen(sb.c_str());
    fprintf(stderr, "posting reply len %" PRIuS "\n", len);
    fprintf(stderr, "posting reply \"%s\".\n", sb.c_str());
    fflush(stderr);

    PostMessage(pp::Var(sb));
    fprintf(stderr, "returning\n");
    fflush(stderr);
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
__attribute__((visibility("default")))
Module* CreateModule() {
  printf("hello world from CreateModule\n"); fflush(NULL);
  return new MyModule();
}

}  // namespace pp
