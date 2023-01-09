// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"

class Status;

typedef base::RepeatingCallback<
    void(const Status&, std::unique_ptr<base::Value>, const std::string&, bool)>
    CommandCallback;

typedef base::RepeatingCallback<
    void(const base::Value::Dict&, const std::string&, const CommandCallback&)>
    Command;

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_H_
