// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_THROW_UNCAUGHT_EXCEPTION_H_
#define COMPONENTS_VIZ_SERVICE_GL_THROW_UNCAUGHT_EXCEPTION_H_

namespace viz {

// Throw that completely unwinds the java stack. In particular, this will not
// trigger a jni CheckException crash.
void ThrowUncaughtException();

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_THROW_UNCAUGHT_EXCEPTION_H_
