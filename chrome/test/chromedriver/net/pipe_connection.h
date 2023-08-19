// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PIPE_CONNECTION_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PIPE_CONNECTION_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/test/chromedriver/net/pipe_connection_win.h"
#elif BUILDFLAG(IS_POSIX)
#include "chrome/test/chromedriver/net/pipe_connection_posix.h"
#endif

#if BUILDFLAG(IS_WIN)
using PipeConnection = PipeConnectionWin;
#elif BUILDFLAG(IS_POSIX)
using PipeConnection = PipeConnectionPosix;
#else
class PipeConnection {};
#endif

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PIPE_CONNECTION_H_
