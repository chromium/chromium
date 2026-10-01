# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import operator
from typing import List


class CommandLineUtility:
    """Helper class to wrap and mutate a list of command line arguments."""

    def __init__(self, args: List[str]):
        self._args = self._normalize_args(args)

    def _normalize_args(self, args: List[str]) -> List[str]:
        # Convert ['--param=value'] to ['--param', 'value'] for consistency.
        normalized_args = []
        for arg in args:
            if arg.startswith('-'):
                normalized_args.extend(arg.split('='))
            else:
                normalized_args.append(arg)
        return normalized_args

    def get_args(self) -> List[str]:
        return self._args

    def set_args(self, args: List[str]):
        self._args = self._normalize_args(args)

    def has_arg(self, arg: str) -> bool:
        return arg in self._args

    def _get_arg_indices(self, target_arg: str) -> List[int]:
        return [i for i, arg in enumerate(self._args) if arg == target_arg]

    def _is_list_arg(self, arg: str) -> bool:
        indices = self._get_arg_indices(arg)
        return len(indices) > 0 and all(
            (i + 1 < len(self._args) and not self._args[i + 1].startswith('--'))
            for i in indices
        )

    def _is_value_arg(self, arg: str) -> bool:
        return operator.countOf(self._args, arg) == 1 and self._is_list_arg(arg)

    def get_flag_value(self, flag: str) -> str:
        assert self._is_value_arg(flag), (
            f"Flag {flag} is not a single-value arg in {self._args}"
        )
        i = self._args.index(flag)
        return self._args[i + 1]

    def get_list_values(self, flag: str) -> List[str]:
        if not self.has_arg(flag):
            return []
        indices = self._get_arg_indices(flag)
        values = []
        for i in indices:
            j = i + 1
            while j < len(self._args) and not self._args[j].startswith('--'):
                values.append(self._args[j])
                j += 1
        return values

    def _should_fail_silently(self, arg: str, throw_if_absent: bool) -> bool:
        return not throw_if_absent and not self.has_arg(arg)

    def set_flag_value(
        self, flag: str, value: str, throw_if_absent: bool = True
    ):
        if self._should_fail_silently(flag, throw_if_absent):
            return
        assert self._is_value_arg(flag), (
            f"Flag {flag} is not a single-value arg in {self._args}"
        )
        i = self._args.index(flag)
        self._args[i + 1] = value

    def update_flag_value(self, flag: str, func, throw_if_absent: bool = True):
        if self._should_fail_silently(flag, throw_if_absent):
            return
        self.set_flag_value(
            flag, func(self.get_flag_value(flag)), throw_if_absent
        )

    def remove_flag(self, flag: str, throw_if_absent: bool = True):
        if self._should_fail_silently(flag, throw_if_absent):
            return
        assert self._is_value_arg(flag), (
            f"Flag {flag} is not a single-value arg in {self._args}"
        )
        i = self._args.index(flag)
        self._args.pop(i)
        self._args.pop(i)

    def append_flag_value(self, flag: str, value: str):
        self._args.append(flag)
        self._args.append(value)

    def append_arg(self, arg: str):
        self._args.append(arg)

    def update_list_arg(self, flag: str, func, throw_if_absent: bool = True):
        if self._should_fail_silently(flag, throw_if_absent):
            return
        assert self._is_list_arg(flag), (
            f"Flag {flag} is not a list arg in {self._args}"
        )
        indices = self._get_arg_indices(flag)
        for i in indices:
            self._args[i + 1] = func(self._args[i + 1])

    def update_all_args(self, func):
        self._args = [func(arg) for arg in self._args]

    def set_arg_at(self, position: int, value: str):
        self._args[position] = value

    def update_arg_at(self, position: int, func):
        self._args[position] = func(self._args[position])
