// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/power_manager_emitter.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "chrome/test/base/chromeos/crosier/upstart.h"

namespace {

const char kPowerdName[] = "powerd";

// Converts the given proto to a comma-separated ASCII hex array expected by
// dbus-send.
std::string ProtoToHexAsciiHexArray(
    const google::protobuf::MessageLite& protobuf) {
  std::string serialized_proto;
  protobuf.SerializeToString(&serialized_proto);

  std::string data;
  for (char c : serialized_proto) {
    if (!data.empty()) {
      data.push_back(',');
    }
    data.append(base::StringPrintf("0x%02x", c));
  }
  return data;
}

}  // namespace

PowerManagerEmitter::PowerManagerEmitter() {
  upstart::StopJob(kPowerdName);
}

PowerManagerEmitter::~PowerManagerEmitter() {
  upstart::StartJob(kPowerdName);
}

bool PowerManagerEmitter::EmitInputEvent(power_manager::InputEvent_Type type) {
  power_manager::InputEvent input_event;
  input_event.set_type(type);
  input_event.set_timestamp(base::TimeTicks::Now().ToInternalValue());

  std::string data = ProtoToHexAsciiHexArray(input_event);

  // This command must be sent as the "power" user.
  auto result = TestSudoHelperClient::ConnectAndRunCommand(
      "sudo -u power dbus-send "
      "--sender=org.chromium.PowerManager "
      "--system "
      "--type=signal "
      "/org/chromium/PowerManager "
      "org.chromium.PowerManager.InputEvent "
      "array:byte:" +
      data);
  return result.return_code == 0;
}
