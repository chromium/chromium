#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import importlib.util

from handlers import priority_list
from dsp import ChromiumUnitTestSimulator


here = os.path.dirname(os.path.abspath(__file__))
os.chdir(here)


def importfile(file):
    spec = importlib.util.spec_from_file_location(file.removesuffix('.py').replace('/', '.'), file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


with open('../audio_device_selection_generated_unittest.cc', 'w') as test_file:
    test_file.write('''// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated test cases from
// chromeos/ash/components/audio/device_selection_test_gen/gen.py.
// DO NOT EDIT.

#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class AudioDeviceSelectionGeneratedTest : public AudioDeviceSelectionTestBase {
};

''')

    tests = []
    for file in sorted(glob.iglob('tests/**/*.py', recursive=True), key=lambda s: s.split('/')):
        tests.append((file, importfile(file).run))

    for name, test in tests:
        sim = ChromiumUnitTestSimulator(priority_list.Handler(), name=name)
        test(sim)
        sim.generate_testcase(file=test_file)


    test_file.write('''
}  // namespace
}  // namespace ash
''')
