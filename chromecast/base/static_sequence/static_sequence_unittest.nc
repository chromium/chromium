// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a no-compile test suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "chromecast/base/static_sequence/static_sequence.h"

namespace util {

struct SequenceA : StaticSequence<SequenceA> {};
struct SequenceB : StaticSequence<SequenceB> {};

void Foo(const SequenceA::Key&);
void Fred(SequenceA::Key&);

class Bar {
 public:
  void Baz(SequenceA::Key&) {}
  void Qux(const SequenceA::Key&) {}
};

void StaticSequenceNoCompileTests() {
  Sequenced<Bar, SequenceB> bar;
#if defined(NCTEST_POST_FUNCTION_TO_WRONG_SEQUENCE) // [r"fatal error: static_assert failed due to requirement 'invalid<util::SequenceB, util::SequenceA>' \"Attempting to post a statically-sequenced task to the wrong static sequence!\""]
  SequenceB::PostTask(base::BindOnce(&Foo));
#elif defined(NCTEST_POST_FUNCTION_WITH_NON_CONST_KEY_REF) // [r".*\"Did you forget to add `const` to the Key parameter of the bound functor\?\""]
  SequenceA::PostTask(base::BindOnce(&Fred));
#elif defined(NCTEST_POST_METHOD_WITH_NON_CONST_KEY_REF)  // [r".*\"Did you forget to add `const` to the Key parameter of the bound functor\?\""]
  bar.Post(FROM_HERE, &Bar::Baz);
#elif defined(NCTEST_POST_METHOD_TO_WRONG_SEQUENCE) // [r"fatal error: static_assert failed due to requirement 'invalid<util::SequenceB, util::SequenceA>' \"Attempting to post a statically-sequenced task to the wrong static sequence!\""]
  bar.Post(FROM_HERE, &Bar::Qux);
#endif
}

}  // namespace util
