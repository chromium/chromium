// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_MEDIUM_INTEGRITY_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_MEDIUM_INTEGRITY_H_

// Drops the current thread to medium integrity.
class ScopedMediumIntegrity final {
 public:
  ScopedMediumIntegrity();
  ScopedMediumIntegrity(const ScopedMediumIntegrity&) = delete;
  ScopedMediumIntegrity& operator=(const ScopedMediumIntegrity&) = delete;
  ~ScopedMediumIntegrity();

  bool Succeeded() const { return impersonating_; }

 private:
  bool impersonating_ = false;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_MEDIUM_INTEGRITY_H_
