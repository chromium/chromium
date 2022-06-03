// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace base {
class DictionaryValue;
class Value;
}

class Status;

typedef base::RepeatingCallback<
    void(const Status&, std::unique_ptr<base::Value>, const std::string&, bool)>
    CommandCallback;

typedef base::RepeatingCallback<void(const base::DictionaryValue&,
                                     const std::string&,
                                     const CommandCallback&)>
    Command;

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_H_
