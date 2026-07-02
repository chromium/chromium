# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gn_utils


class Override:

    def apply(self, target):
        raise NotImplementedError()


class AppendSet(Override):

    def __init__(self, attribute, values):
        self.attribute = attribute
        self.values = values

    def apply(self, target):
        curr = getattr(target, self.attribute)
        if curr is None:
            setattr(target, self.attribute, set(self.values))
        else:
            assert isinstance(
                curr, set
            ), f"Expected set for {self.attribute} on {target.name}, got {type(curr)}"
            curr.update(self.values)


class ExtendList(Override):

    def __init__(self, attribute, values):
        self.attribute = attribute
        self.values = values

    def apply(self, target):
        curr = getattr(target, self.attribute)
        if curr is None:
            setattr(target, self.attribute, list(self.values))
        else:
            assert isinstance(
                curr, list
            ), f"Expected list for {self.attribute} on {target.name}, got {type(curr)}"
            curr.extend(self.values)


class SetValue(Override):

    def __init__(self, attribute, value):
        self.attribute = attribute
        self.value = value

    def apply(self, target):
        setattr(target, self.attribute, self.value)


class PatchDict(Override):

    def __init__(self, attribute, key, value):
        self.attribute = attribute
        self.key = key
        self.value = value

    def apply(self, target):
        curr = getattr(target, self.attribute)
        assert isinstance(
            curr, dict
        ), f"Expected dict for {self.attribute} on {target.name}, got {type(curr)}"
        if self.key in curr and isinstance(curr[self.key], set) and isinstance(
                self.value, set):
            curr[self.key].update(self.value)
        elif self.key in curr and isinstance(
                curr[self.key], list) and isinstance(self.value, list):
            curr[self.key].extend(self.value)
        else:
            curr[self.key] = self.value


class ArchOverride(Override):

    def __init__(self, arch, nested_overrides):
        self.arch = arch
        self.nested_overrides = nested_overrides

    def apply(self, target):
        target_variant = target.target.get(self.arch)
        assert target_variant is not None, f"Arch {self.arch} not found on {target.name}"
        for override in self.nested_overrides:
            override.apply(target_variant)


class TranslationContext:

    def __init__(self, import_channel: str):
        self.import_channel = import_channel
        self.module_prefix = f'{import_channel}_cronet_'
        self.include_dirs_denylist = [
            f'external/cronet/{import_channel}/third_party/zlib/',
        ]
        self.cc_defaults_module = f'{self.module_prefix}cc_defaults'
        self.java_framework_defaults_module = f'{self.module_prefix}java_framework_defaults'
        self.tree_path = f'external/cronet/{import_channel}'
        self.additional_args = self._initialize_additional_args()

    def _initialize_additional_args(self):
        args = {
            '//net/third_party/quiche:net_quic_test_tools_proto_gen#h': [
                AppendSet('export_include_dirs', {
                    "net/third_party/quiche/src",
                })
            ],
            '//net/third_party/quiche:net_quic_test_tools_proto_gen#h#testing':
            [
                AppendSet('export_include_dirs', {
                    "net/third_party/quiche/src",
                })
            ],
            # TODO: fix upstream. Both //base:base and
            # //base/allocator/partition_allocator:partition_alloc do not create a
            # dependency on gtest despite using gtest_prod.h.
            '//base:base': [
                AppendSet('header_libs', {
                    'libgtest_prod_headers',
                }),
                AppendSet('export_header_lib_headers', {
                    'libgtest_prod_headers',
                }),
            ],
            '//base/allocator/partition_allocator:partition_alloc': [
                AppendSet('header_libs', {
                    'libgtest_prod_headers',
                }),
            ],
            # TODO(b/309920629): Remove once upstreamed.
            '//components/cronet/android:cronet_api_java#unfiltered': [
                AppendSet(
                    'srcs', {
                        'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
                        'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
                    }),
            ],
            '//components/cronet/android:cronet_javatests#unfiltered#testing':
            [
                AppendSet('static_libs', {
                    'net-tests-utils-host-device-common',
                }),
            ],
            '//components/cronet/android:cronet#testing': [
                ArchOverride('android_riscv64',
                             [SetValue('stem', "libmainlinecronet_riscv64")]),
                SetValue(
                    'comment', """TODO: remove stem for riscv64
// This is essential as there can't be two different modules
// with the same output. We usually got away with that because
// the non-testing Cronet is part of the Tethering APEX and the
// testing Cronet is not part of the Tethering APEX which made them
// look like two different outputs from the build system perspective.
// However, Cronet does not ship to Tethering APEX for RISCV64 which
// raises the conflict. Once we start shipping Cronet for RISCV64,
// this can be removed."""),
            ],
            '//third_party/netty-tcnative:netty-tcnative-so#testing':
            [ExtendList('cflags', ["-Wno-error=pointer-bool-conversion"])],
            '//third_party/apache-portable-runtime:apr#testing': [
                ExtendList('cflags', [
                    "-Wno-incompatible-pointer-types-discards-qualifiers",
                ])
            ],
            # TODO(b/324872305): Remove when gn desc expands public_configs and update code to propagate the
            # include_dir from the public_configs
            # We had to add the export_include_dirs for each target because soong generates each header
            # file in a specific directory named after the target.
            '//base/allocator/partition_allocator/src/partition_alloc:chromecast_buildflags':
            [
                AppendSet('export_include_dirs', {
                    "base/allocator/partition_allocator/src/",
                })
            ],
            '//base/allocator/partition_allocator/src/partition_alloc:chromeos_buildflags':
            [
                AppendSet('export_include_dirs', {
                    "base/allocator/partition_allocator/src/",
                })
            ],
            '//base/allocator/partition_allocator/src/partition_alloc:debugging_buildflags':
            [
                AppendSet('export_include_dirs', {
                    "base/allocator/partition_allocator/src/",
                })
            ],
            '//base/allocator/partition_allocator/src/partition_alloc:buildflags':
            [
                AppendSet('export_include_dirs', {
                    ".",
                    "base/allocator/partition_allocator/src/",
                })
            ],
            '//base/allocator/partition_allocator/src/partition_alloc:raw_ptr_buildflags':
            [
                AppendSet('export_include_dirs', {
                    "base/allocator/partition_allocator/src/",
                })
            ],
            # Protobuf depends on Unsafe class which is used to perform unsafe native methods. This class is not
            # available in the public API provided by the android platform. It's only available by compiling
            # against `core_current` and adding `libcore_private.stubs` as a dependency.
            # defaults have to be removed to prevent sdk_version collision.
            '//third_party/protobuf:proto_runtime_lite_java#unfiltered': [
                AppendSet('libs', {
                    "libcore_private.stubs",
                }),
                SetValue('defaults', None),
                SetValue('sdk_version', 'core_current'),
            ],
            '//base:base_java_test_support#testing': [
                PatchDict('errorprone', 'javacflags', {
                    "-Xep:ReturnValueIgnored:WARN",
                })
            ],
            '//third_party/perfetto/gn:gen_buildflags': [
                AppendSet('export_include_dirs', {
                    "third_party/perfetto/build_config/",
                })
            ],
            # See https://crbug.com/517894073#comment5
            '//third_party/boringssl:raw_bssl_sys_bindings': [
                AppendSet('export_include_dirs', {
                    "third_party/boringssl/src/include",
                }),
                AppendSet('local_include_dirs', {
                    "third_party/boringssl/src/include",
                })
            ],
            # TODO: https://crbug.com/418746360 - Handle //base:build_date_internal
            # for os:linux_glibc.
            '//base:build_date_internal#testing':
            [SetValue('host_supported', True)],
            '//components/cronet/android:cronet#nontesting':
            [SetValue('afdo', True)],
        }
        return args
