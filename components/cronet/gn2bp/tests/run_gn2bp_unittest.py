#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import pathlib
import sys
import tempfile
import unittest
import unittest.mock

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir
    )
)

sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.gn2bp.gen_android_bp as gen_android_bp  # noqa: E402
import components.cronet.gn2bp.gn_utils as gn_utils  # noqa: E402
import components.cronet.gn2bp.run_gn2bp as run_gn2bp  # noqa: E402
import components.cronet.tools.breakages_constants as breakages_constants  # noqa: E402


class TestRunGN2BPUnitTest(unittest.TestCase):
    def setUp(self):
        run_gn2bp.GetCurrentCheckoutBranchStableState.cache_clear()

    def test_bad_change_id_no_good_change_id_should_throw(self):
        self.assertRaisesRegex(
            RuntimeError,
            'Stopping the import: there is a breakage that has not been fixed yet.',
            run_gn2bp.validate_release,
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: [],
                }
            ],
            {'foo': 0},
        )

    def test_bad_change_id_and_no_good_change_id_but_but_both_not_in_history_should_throw(
        self,
    ):
        self.assertRaisesRegex(
            RuntimeError,
            'Stopping the import: there is a breakage that has not been fixed yet.',
            run_gn2bp.validate_release,
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: [],
                }
            ],
            {},
        )

    def test_bad_change_id_and_good_change_id_but_but_both_not_in_history_should_not_throw(
        self,
    ):
        run_gn2bp.validate_release(
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar'],
                }
            ],
            {},
        )

    def test_bad_change_id_and_good_change_id_but_but_bad_not_in_history_should_not_throw(
        self,
    ):
        run_gn2bp.validate_release(
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar'],
                }
            ],
            {'bar': 0},
        )

    def test_bad_change_id_and_good_change_id_but_not_in_history_should_throw(
        self,
    ):
        self.assertRaisesRegex(
            RuntimeError,
            'Stopping the import: the current checkout includes a breaking change, but not its fix.',
            run_gn2bp.validate_release,
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar'],
                }
            ],
            {'foo': 0},
        )

    def test_sort_versions(self):
        self.assertEqual(
            run_gn2bp.sort_versions(
                ["123.0.1000.0", "123.0.999.0", "123.0.1001.0"]
            ),
            ["123.0.999.0", "123.0.1000.0", "123.0.1001.0"],
        )

    def test_bad_change_id_and_good_change_id_in_history_should_not_throw(self):
        run_gn2bp.validate_release(
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar'],
                }
            ],
            {'bar': 0, 'foo': 1},
        )

    def test_bad_change_id_and_good_change_id_but_before_bad_change_id_should_throw(
        self,
    ):
        self.assertRaisesRegex(
            RuntimeError,
            'the local history shows a bad change ID that is more recent than its fix',
            run_gn2bp.validate_release,
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: ['bar'],
                }
            ],
            {'bar': 1, 'foo': 0},
        )

    def test_bad_change_id_but_two_good_change_ids_should_throw(self):
        self.assertRaisesRegex(
            RuntimeError,
            'Multiple good change IDs are only necessary when a fix has to be cherry-picked into a release',
            run_gn2bp.validate_release,
            [
                {
                    breakages_constants.BAD_CHANGE_ID_TXT: 'foo',
                    breakages_constants.GOOD_CHANGE_IDS_TXT: [
                        'bar',
                        'bar_branch',
                    ],
                }
            ],
            {'bar': 0, 'bar_branch': 1, 'foo': 2},
        )

    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_current_checkout_version_string'
    )
    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_latest_stable_version_string'
    )
    def test_GetCurrentCheckoutBranchStableState_stable(
        self, mock_latest_stable, mock_current
    ):
        mock_current.return_value = "123.0.6300.0"
        mock_latest_stable.return_value = "123.0.6300.0"
        self.assertEqual(
            run_gn2bp.GetCurrentCheckoutBranchStableState(),
            run_gn2bp.BranchStableState.BRANCH_IS_STABLE,
        )

    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_current_checkout_version_string'
    )
    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_latest_stable_version_string'
    )
    def test_GetCurrentCheckoutBranchStableState_old_stable(
        self, mock_latest_stable, mock_current
    ):
        mock_current.return_value = "123.0.6200.0"
        mock_latest_stable.return_value = "123.0.6300.0"
        self.assertEqual(
            run_gn2bp.GetCurrentCheckoutBranchStableState(),
            run_gn2bp.BranchStableState.BRANCH_IS_OLD_STABLE,
        )

    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_current_checkout_version_string'
    )
    @unittest.mock.patch(
        'components.cronet.gn2bp.run_gn2bp._get_latest_stable_version_string'
    )
    def test_GetCurrentCheckoutBranchStableState_not_yet_stable(
        self, mock_latest_stable, mock_current
    ):
        mock_current.return_value = "123.0.6400.0"
        mock_latest_stable.return_value = "123.0.6300.0"
        self.assertEqual(
            run_gn2bp.GetCurrentCheckoutBranchStableState(),
            run_gn2bp.BranchStableState.BRANCH_IS_NOT_YET_STABLE,
        )

    def test_get_build_number_from_version_string(self):
        self.assertEqual(
            run_gn2bp._get_build_number_from_version_string("123.0.6300.0"),
            6300,
        )
        self.assertEqual(
            run_gn2bp._get_build_number_from_version_string("1.2.3.4"), 3
        )
        self.assertEqual(
            run_gn2bp._get_build_number_from_version_string(
                "100.200.30000.400"
            ),
            30000,
        )

    def test_extract_copybara_paths(self):
        gn = gn_utils.GnParser({}, {})

        rust_lib = gn_utils.GnParser.Target(
            "//third_party/rust/small_ctor/v0_1:lib", "rust_library"
        )
        rust_lib.build_file_path = "third_party/rust/small_ctor/v0_1"
        rust_lib.crate_root = (
            "//third_party/rust/chromium_crates_io/vendor/"
            "small_ctor-v0_1/src/lib.rs"
        )
        gn.all_targets[rust_lib.name] = rust_lib

        cxx_action = gn_utils.GnParser.Target(
            "//components/prefs:rust_bridge", "action"
        )
        cxx_action.build_file_path = "components/prefs"
        cxx_action.script = (
            "//third_party/rust/cxx/chromium_integration/run_cxxbridge.py"
        )
        cxx_action.common.sources.add(
            "//third_party/rust/chromium_crates_io/vendor/cxx-v1/include/cxx.h"
        )
        cxx_action.common.sources.add("//base/third_party/nspr/prtime.cc")
        cxx_action.common.data.add("//net/data/")
        cxx_action.common.data.add(
            "//components/cronet/android/test/test_server/data/"
        )
        cxx_action.common.inputs.add("//third_party/jdk/bin/java")
        cxx_action.common.inputs.add("//out/tmp/gen/generated.h")
        cxx_action.common.inputs.add("//components/prefs/prefs.gni")
        cxx_action.common.include_dirs.add("//third_party/empty_inc/include")
        gn.all_targets[cxx_action.name] = cxx_action

        libcxx = gn_utils.GnParser.Target(
            "//buildtools/third_party/libc++:libc++", "static_library"
        )
        libcxx.common.inputs.update(
            {
                "//buildtools/third_party/libc++/__assertion_handler",
                "//buildtools/third_party/libc++/__config_site",
            }
        )
        libcxx.common.cflags.append(
            "-isystem../../third_party/libc++/src/include"
        )
        libcxx.common.include_dirs.add("//third_party/libc++/src/src/")
        gn.all_targets[libcxx.name] = libcxx

        top_bp = gen_android_bp.soong_ast.Blueprint()
        ctx = gen_android_bp.translation_context.TranslationContext("tot")
        for extra_mod in gen_android_bp.create_extras_modules(gn, ctx):
            top_bp.add_module(extra_mod)
        mod = gen_android_bp.soong_ast.create_module(
            "cc_genrule", "tot_cronet_prefs_gen", "//components/prefs:gen", ctx
        )
        mod.srcs = {":tot_cronet_other_mod", "components/prefs/pref_service.cc"}
        mod.tool_files = {"tools/metrics/histograms/enums.xml"}
        mod.export_include_dirs = {"third_party/empty_inc/include"}
        top_bp.add_module(mod)

        final_bps = {
            "": top_bp,
            "components/prefs": gen_android_bp.soong_ast.Blueprint(
                "components/prefs"
            ),
        }

        with (
            tempfile.TemporaryDirectory() as tmp_dir,
            unittest.mock.patch.object(
                gen_android_bp, "REPOSITORY_ROOT", tmp_dir
            ),
        ):
            readme_file = (
                pathlib.Path(tmp_dir)
                / "third_party/rust/small_ctor/v0_1/README.chromium"
            )
            readme_file.parent.mkdir(parents=True, exist_ok=True)
            readme_file.write_text(
                "Name: small_ctor\n"
                "URL: https://crates.io/crates/small_ctor\n"
                "Version: 0.1.2\n"
                "License: Apache-2.0\n"
                "License File: //third_party/rust/chromium_crates_io/vendor/"
                "small_ctor-v0_1/LICENSE\n"
            )
            pref_cc = pathlib.Path(tmp_dir) / "components/prefs/pref_service.cc"
            pref_cc.parent.mkdir(parents=True, exist_ok=True)
            pref_cc.write_text(
                '#include "components/prefs/pref_service.h"\n'
                '#include "third_party/zlib/zlib.h"\n'
            )
            zlib_h = pathlib.Path(tmp_dir) / "third_party/zlib/zlib.h"
            zlib_h.parent.mkdir(parents=True, exist_ok=True)
            zlib_h.write_text("// zlib header\n")
            pref_h = pathlib.Path(tmp_dir) / "components/prefs/pref_service.h"
            pref_h.write_text('#include "unlisted_internal.h"\n')
            unlisted_h = (
                pathlib.Path(tmp_dir) / "components/prefs/unlisted_internal.h"
            )
            unlisted_h.write_text("// internal header\n")
            empty_inc_dir = (
                pathlib.Path(tmp_dir) / "third_party/empty_inc/include"
            )
            empty_inc_dir.mkdir(parents=True, exist_ok=True)
            generated_paths = set(
                gen_android_bp.extract_copybara_paths(
                    gn, top_bp, final_bps, ctx
                )
            )
        expected_subset = {
            "base/third_party/nspr/prtime.cc",
            "buildtools/third_party/libc++/__assertion_handler",
            "buildtools/third_party/libc++/__config_site",
            "components/cronet/android/test/test_server/data/**",
            "components/prefs/Android.bp",
            "components/prefs/pref_service.cc",
            "components/prefs/pref_service.h",
            "components/prefs/unlisted_internal.h",
            "net/data/**",
            "third_party/boringssl/**",
            "third_party/empty_inc/include/**",
            "third_party/libc++/src/include/**",
            "third_party/libc++/src/src/**/*.ipp",
            "third_party/rust/chromium_crates_io/vendor/cxx-v1/include/cxx.h",
            "third_party/rust/chromium_crates_io/vendor/small_ctor-v0_1/LICENSE",
            "third_party/rust/chromium_crates_io/vendor/small_ctor-v0_1/METADATA",
            "third_party/rust/chromium_crates_io/vendor/small_ctor-v0_1/MODULE_LICENSE_APACHE_2_0",
            "third_party/rust/chromium_crates_io/vendor/small_ctor-v0_1/src/lib.rs",
            "third_party/rust/small_ctor/v0_1/README.chromium",
            "tools/metrics/histograms/enums.xml",
        }
        self.assertTrue(expected_subset.issubset(generated_paths))
        self.assertNotIn("third_party/jdk/bin/java", generated_paths)
        self.assertNotIn(
            "third_party/rust/cxx/chromium_integration/run_cxxbridge.py",
            generated_paths,
        )
        self.assertNotIn("third_party/zlib/zlib.h", generated_paths)
        self.assertNotIn("out/tmp/gen/generated.h", generated_paths)
        self.assertNotIn("components/prefs/prefs.gni", generated_paths)
        self.assertNotIn(":tot_cronet_other_mod", generated_paths)

    def test_write_copybara_paths_file(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            gen_android_bp.write_copybara_paths_file(
                tmp_dir,
                [
                    "components/prefs/pref_service.cc",
                    "net/data/**",
                    "third_party/rust/small_ctor/v0_1/README.chromium",
                ],
            )
            sky_path = (
                pathlib.Path(tmp_dir) / "copybara_generated_paths.bara.sky"
            )
            self.assertTrue(sky_path.exists())
            content = sky_path.read_text()
            self.assertIn(
                'GENERATED_ORIGIN_FILES = [\n'
                '    "components/prefs/pref_service.cc",\n'
                '    "third_party/rust/small_ctor/v0_1/README.chromium",\n'
                ']\n\n'
                'GENERATED_ORIGIN_GLOBS = [\n'
                '    "net/data/**",\n'
                ']\n',
                content,
            )

    def test_parse_gn_desc_unwraps_wrapper_targets(self):
        tc = "//build/toolchain/android:android_clang_arm64"
        gn_desc = {
            "//test:root_group": {
                "type": "group",
                "toolchain": tc,
                "deps": [
                    "//test:gtest_wrapper",
                    "//test:instrumentation_apk",
                    "//test:repackage_jars",
                ],
            },
            "//test:gtest_wrapper": {
                "type": "action",
                "script": "//build/util/generate_wrapper.py",
                "toolchain": tc,
                "deps": ["//test:gtest_wrapper__apk"],
            },
            "//test:gtest_wrapper__apk": {
                "type": "group",
                "toolchain": tc,
                "deps": [
                    "//test:gtest_wrapper__library",
                    "//test:gtest_wrapper__apk__java",
                ],
            },
            "//test:gtest_wrapper__library": {
                "type": "shared_library",
                "toolchain": tc,
                "sources": ["//test/unittest.cc"],
                "deps": [],
            },
            "//test:gtest_wrapper__apk__java": {
                "type": "group",
                "toolchain": tc,
                "metadata": {"java_library_deps": []},
                "deps": [
                    "//test:helper_java",
                    "//test:empty_resources",
                    "//test:gtest_wrapper__apk__java_single_jar_java",
                    "//test:gtest_wrapper__apk__java__dex",
                ],
            },
            "//test:gtest_wrapper__apk__java__dex": {
                "type": "action",
                "script": "//build/android/gyp/dex.py",
                "toolchain": tc,
                "deps": [],
            },
            "//test:helper_java": {
                "type": "group",
                "toolchain": tc,
                "metadata": {
                    "java_library_inputs": ["//test/Helper.java"],
                    "java_library_deps": [],
                },
                "deps": [],
            },
            "//test:empty_resources": {
                "type": "group",
                "toolchain": tc,
                "metadata": {
                    "java_library_inputs": [],
                    "java_library_deps": [],
                },
                "deps": [],
            },
            "//test:instrumentation_apk": {
                "type": "action",
                "script": "//build/android/gyp/generate_android_wrapper.py",
                "toolchain": tc,
                "deps": ["//test:instrumentation_apk__write_deps"],
            },
            "//test:instrumentation_apk__write_deps": {
                "type": "group",
                "toolchain": tc,
                "deps": ["//test:instrumentation_apk__test_apk"],
            },
            "//test:instrumentation_apk__test_apk": {
                "type": "group",
                "toolchain": tc,
                "deps": ["//test:native_test_so"],
            },
            "//test:native_test_so": {
                "type": "shared_library",
                "toolchain": tc,
                "sources": ["//test/native_test.cc"],
                "deps": [],
            },
            "//test:repackage_jars": {
                "type": "action",
                "script": "//build/android/gyp/rename_java_classes.py",
                "toolchain": tc,
                "deps": ["//test:repackage_jars_zip"],
            },
            "//test:repackage_jars_zip": {
                "type": "action",
                "script": "//build/android/gyp/zip.py",
                "toolchain": tc,
                "deps": [
                    "//test:prebuilt_jar_java",
                    "//test:repackage_jars_zip__build_config_crbug_908819",
                ],
            },
            "//test:prebuilt_jar_java": {
                "type": "group",
                "toolchain": tc,
                "metadata": {
                    "java_library_inputs": ["//out/Release/gen/repackaged.jar"],
                    "java_library_deps": ["//test:packaged_java"],
                },
                "deps": [],
            },
            "//test:packaged_java": {
                "type": "group",
                "toolchain": tc,
                "metadata": {
                    "java_library_inputs": ["//test/Packaged.java"],
                    "java_library_deps": [],
                },
                "deps": [],
            },
        }

        gn = gn_utils.GnParser({}, {})
        gn.parse_gn_desc(gn_desc, "//test:root_group", is_test_target=True)
        gn.propagate_properties()

        ctx = gen_android_bp.translation_context.TranslationContext("tot")
        bp = gen_android_bp.soong_ast.Blueprint()
        with unittest.mock.patch.object(
            gen_android_bp.translators,
            "_parse_pydeps",
            return_value=["build/android/gyp/filter_zip.py"],
        ):
            modules = gen_android_bp.translators.create_modules_from_target(
                bp,
                gn,
                "//test:root_group__testing",
                None,
                is_test_target=True,
                context=ctx,
            )
        root_module_names = {m.name for m in modules}
        self.assertEqual(
            root_module_names,
            {
                "tot_cronet_test_gtest_wrapper__library__testing",
                "tot_cronet_test_helper_java__testing",
                "tot_cronet_test_native_test_so__testing",
                "tot_cronet_test_packaged_java__testing",
            },
        )
        self.assertIn(
            "tot_cronet_test_gtest_wrapper__library__testing", bp.modules
        )
        self.assertIn("tot_cronet_test_helper_java__testing", bp.modules)
        self.assertIn("tot_cronet_test_native_test_so__testing", bp.modules)
        self.assertIn("tot_cronet_test_packaged_java__testing", bp.modules)
        self.assertNotIn("tot_cronet_test_empty_resources__testing", bp.modules)
        self.assertNotIn(
            "tot_cronet_test_prebuilt_jar_java__testing", bp.modules
        )

    def test_create_extras_modules_derives_from_gn(self):
        gn = gn_utils.GnParser({}, {})

        def _add_target(name, target_type, testonly=False, deps=(), data=()):
            target = gn_utils.GnParser.Target(name, target_type)
            target.testonly = testonly
            target.common.deps.update(deps)
            target.common.data.update(data)
            gn.all_targets[name] = target
            return target

        _add_target("//components/cronet/android:cronet", "shared_library")
        _add_target(
            "//components/cronet/android:cronet_api_java", "java_library"
        )
        _add_target(
            "//components/cronet/android:cronet_build_config_java",
            "java_library",
        )
        _add_target(
            "//components/cronet/android:cronet_impl_native_java",
            "java_library",
        )
        _add_target(
            "//components/cronet/android:cronet_final_jni_java", "java_library"
        )
        _add_target(
            "//components/cronet/android:gn2bp_production_targets",
            "group",
            deps=[
                "//components/cronet/android:cronet",
                "//components/cronet/android:cronet_api_java",
                "//components/cronet/android:cronet_build_config_java",
                "//components/cronet/android:cronet_impl_native_java",
                "//components/cronet/android:cronet_final_jni_java",
            ],
        )

        _add_target(
            "//components/cronet/android:cronet_tests_final_jni_java__testing",
            "java_library",
            testonly=True,
        )
        _add_target(
            "//components/cronet/android:repackage_test_jars__testing",
            "group",
            testonly=True,
            deps=[
                "//components/cronet/android:cronet_tests_final_jni_java__testing",
            ],
        )

        _add_target(
            "//net/android:net_java__testing",
            "java_library",
            testonly=False,
        )
        _add_target(
            "//net/android:net_java_test_support__testing",
            "java_library",
            testonly=True,
        )
        _add_target(
            "//net/android:net_tests_java__testing",
            "java_library",
            testonly=True,
        )
        _add_target(
            "//net/android:dummy_spnego_authenticator_java__testing",
            "java_library",
            testonly=True,
        )
        _add_target(
            "//net:net_unittests__testing",
            "group",
            testonly=True,
            deps=[
                "//net/android:net_java__testing",
                "//net/android:net_java_test_support__testing",
                "//net/android:net_tests_java__testing",
                "//net/android:dummy_spnego_authenticator_java__testing",
            ],
        )
        _add_target(
            "//net:net_unittests__library__testing",
            "shared_library",
            testonly=True,
            data=[
                "//net/data/ssl/",
                "//net/data/test.html",
            ],
        )
        _add_target(
            "//components/cronet/android:cronet_javatests__testing",
            "java_library",
            testonly=True,
            data=[
                "//components/cronet/android/test/test_server/data/",
                "//net/data/ssl/certificates/",
            ],
        )
        libcxx = _add_target(
            "//buildtools/third_party/libc++:libc++",
            "static_library",
        )
        libcxx.common.inputs.update(
            {
                "//buildtools/third_party/libc++/__assertion_handler",
                "//buildtools/third_party/libc++/__config_site",
            }
        )
        libcxx.arch["android_arm64"] = gn_utils.GnParser.Target.Arch()
        libcxx.arch["android_arm64"].cflags.extend(
            [
                "-mbranch-protection=standard",
                "-isystem../../third_party/libc++/src/include",
            ]
        )
        libcxx.arch["android_x86"] = gn_utils.GnParser.Target.Arch()
        libcxx.arch["android_x86"].cflags.extend(
            [
                "-msse3",
                "-isystem../../third_party/libc++/src/include",
            ]
        )
        libcxx._finalize_attribute("cflags")
        libcxx.common.include_dirs.add("//third_party/libc++/src/src/")

        ctx = gen_android_bp.translation_context.TranslationContext("tot")
        mods = {
            m.name: m for m in gen_android_bp.create_extras_modules(gn, ctx)
        }

        self.assertIn("tot_cronet_api_java", mods)
        self.assertEqual(
            mods["tot_cronet_api_java"].static_libs,
            {"tot_cronet_components_cronet_android_cronet_api_java"},
        )
        self.assertEqual(mods["tot_cronet_java_tests"].type, "java_library")
        self.assertIsNone(
            getattr(mods["tot_cronet_java_tests"], "manifest", None)
        )
        self.assertEqual(
            mods["tot_cronet_libcxx_headers"].srcs,
            {
                "buildtools/third_party/libc++/__assertion_handler",
                "buildtools/third_party/libc++/__config_site",
                "third_party/libc++/src/include/*",
                "third_party/libc++/src/src/**/*.ipp",
            },
        )
        self.assertEqual(
            mods["tot_cronet_libcxx_headers"].exclude_srcs,
            {"third_party/libc++/src/include/*.*"},
        )
        self.assertEqual(
            mods["tot_cronet_java"].static_libs,
            {
                "tot_cronet_components_cronet_android_cronet_build_config_java",
                "tot_cronet_components_cronet_android_cronet_impl_native_java",
                "tot_cronet_components_cronet_android_cronet_final_jni_java",
            },
        )
        self.assertEqual(
            mods["tot_cronet_java__testing"].static_libs,
            {
                "tot_cronet_components_cronet_android_cronet_build_config_java__testing",
                "tot_cronet_components_cronet_android_cronet_impl_native_java__testing",
                "tot_cronet_components_cronet_android_cronet_final_jni_java__testing",
                "tot_cronet_components_cronet_android_cronet_tests_final_jni_java__testing",
            },
        )
        self.assertEqual(
            mods["tot_cronet_for_testing_net_java"].static_libs,
            {
                "tot_cronet_net_android_net_java_test_support__testing",
                "tot_cronet_net_android_net_tests_java__testing",
                "tot_cronet_net_android_dummy_spnego_authenticator_java__testing",
            },
        )
        self.assertEqual(
            mods["tot_cronet_net_unittests_data"].srcs,
            {
                "net/data/ssl/**/*",
                "net/data/test.html",
            },
        )
        self.assertEqual(
            mods["tot_cronet_javatests_resources"].srcs,
            {
                "components/cronet/android/test/test_server/data/**/*",
                "net/data/ssl/certificates/**/*",
            },
        )
        self.assertIn(
            "tot_cronet_components_cronet_android_cronet_javatests__testing",
            mods["tot_cronet_java_tests"].static_libs,
        )
        self.assertTrue(
            mods["tot_cronet_java__testing"].static_libs.issubset(
                mods["tot_cronet_java_tests"].static_libs
            )
        )
        self.assertNotIn(
            "tot_cronet_java__testing",
            mods["tot_cronet_java_tests"].static_libs,
        )
        self.assertIn(
            "tot_cronet_api_java",
            mods["tot_cronet_java_tests"].libs,
        )


if __name__ == '__main__':
    # This allows you to run the file directly
    unittest.main()
