// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/strings/sys_string_conversions.h"
#import "chrome/updater/mac/setup/ks_tickets.h"

namespace updater {
namespace {

int ReadTicketStore(char* path) {
  @autoreleasepool {
    NSDictionary<NSString*, KSTicket*>* store =
        [KSTicketStore readStoreWithPath:[NSString stringWithUTF8String:path]];
    for (NSString* key in store) {
      std::cout << "------ Key " << base::SysNSStringToUTF8(key) << "\n"
                << base::SysNSStringToUTF8(
                       [[store objectForKey:key] description])
                << "\n\n";
    }
  }
  return 0;
}

}  // namespace
}  // namespace updater

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: keystone_ticket_reader path/to/Keystone.ticketstore\n";
    return 1;
  }
  return updater::ReadTicketStore(argv[1]);
}
