#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import dataclasses
import json
import logging
import os
import sys
import unicodedata
import xml.etree.ElementTree
from typing import Any, Dict, Generator, List, Optional, Sequence, Set, Tuple

# Add extra dependencies to the python path.
_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 5))
sys.path.append(os.path.join(_CHROME_SOURCE, 'build'))

import action_helpers

# Initialize logger.
logging.basicConfig(stream=sys.stdout, level=logging.ERROR)
LOGGER = logging.getLogger(__name__)

# List of unicode ranges for each symbol group (ranges are inclusive).
SYMBOLS_GROUPS = {
    'Arrows': [
        # Arrows Unicode Block.
        (0x2190, 0x21ff),
        # Supplemental Arrows-C Unicode Block.
        # Note: There are unassigned code points in the block which are
        # automatically skipped by the script.
        (0x1f800, 0x1f8ff),
    ],
    'Bullet/Stars': [
        # Some rows from Miscellaneous Symbols and Arrows Unicode block.
        (0x2b20, 0x2b2f),
        (0x2b50, 0x2b5f),
        (0x2b90, 0x2b9f),
        (0x2bb0, 0x2bbf),
        (0x2bc0, 0x2bcf),
    ],
    'Currency': [
        # Currency Unicode Block.
        (0x20a0, 0x20bf),
        # Currency from Latin-1 Supplement.
        (0x00A2, 0x00A5),
    ],
    'Letterlike': [
        # Letterlike Symbols Unicode Block.
        (0x2100, 0x210f),
    ],
    'Math': [
        # Greek Letters and Symbols from Mathematical and Alphanumeric
        # Symbols Unicode Block.
        # Normal Capital Letters.
        (0x0391, 0x0391 + 25),
        # Normal Small Letters.
        (0x03b1, 0x03b1 + 25),
        # Mathematical Operators
        (0x2200, 0x2235),
        (0x2260, 0x228b),
        # Fractions from Latin-1 Supplement
        (0x00BC, 0x00BE),
        # Degree symbol from Latin-1 Supplement
        (0x00B0, 0x00B0),
    ],
    'Miscellaneous': [
        # Miscellaneous Symbols Unicode Block.
        (0x2600, 0x26ff),
        # Copyright
        (0x00A9, 0x00A9),
        # Registered
        (0x00AE, 0x00AE),
    ],
}

# List of unicode ranges (inclusive) for each search only symbol group.
SEARCH_ONLY_SYMBOLS_GROUPS = {
    'Letterlike': [
        # Letterlike Symbols Unicode Block.
        (0x2110, 0x214f),
    ],
    'Math': [
        # Greek Letters and Symbols from Mathematical and Alphanumeric
        # Symbols Unicode Block.
        # Bold Capital Letters.
        (0x1D6A8, 0x1D6A8 + 25),
        # Italic Capital Letters.
        (0x1D6E2, 0x1D6E2 + 25),
        # Bold-Italic Capital Letters.
        (0x1D71C, 0x1D71C + 25),
        # Mathematical Operators
        (0x2236, 0x225f),
        (0x228c, 0x22df),
        # Number Forms.
        (0x2150, 0x218B),
    ],
    'Miscellaneous': [
        # Miscellaneous Symbols Unicode Block.
        (0x2300, 0x23CF),
        # Thaana.
        (0x0780, 0x07B1),
    ],
    'Modifier Letter': [
        # Spacing Modifier Letters.
        (0x02B0, 0x02FF),
    ],
    'Phonetic': [
        # Phonetic Extensions.
        (0x1D00, 0x1D7F),
    ],
}

# Set of unicode symbols that do not render with fonts available on ChromeOS
INVALID_SYMBOLS = set([
    '\u2BBA',
    '\u2BBB',
    '\u2BBC',
    '\u2B97',
    '\u2BC9',
    '\U0001F8B0',
    '\U0001F8B1',
])

# Custom search keywords for symbols.
# By default, symbols do not have search keywords.
# These are "shortcut" style search keywords based off compose key to provide a
# fast way to access common symbols.
CUSTOM_KEYWORDS = {
    '⅐': ['17'],
    '⅓': ['13'],
    '⅔': ['23'],
    '½': ['12'],
    '¼': ['14'],
    '¾': ['34'],
    '⅕': ['15'],
    '⅖': ['25'],
    '⅗': ['35'],
    '⅘': ['45'],
    '⅙': ['16'],
    '⅛': ['18'],
    '⅜': ['38'],
    '⅝': ['58'],
    '⅞': ['78'],
    '©': ['oc', 'co'],
    '®': ['or', 'ro'],
    '₠': ['CE'],
    '₡': ['C/', '/C'],
    '₢': ['Cr'],
    '₣': ['Fr'],
    '¢': ['|c', 'c|', 'c/', '/c'],
    '£': ['L-', '-L'],
    '₥': ['m/', '/m'],
    '₦': ['N=', '=N'],
    '₧': ['P', 't'],
    '₨': ['R', 's'],
    '₩': ['W=', '=W'],
    '₫': ['=d', 'd='],
    '€': ['C=', '=C', '=E', 'E='],
    '¥': ['Y=', '=Y', 'Y-', '-Y'],
    '♩': ['#q'],
    '♪': ['#e'],
    '♫': ['#E'],
    '♬': ['#S'],
    '♭': ['#b'],
    '♮': ['#f'],
    '♯': ['##'],
}


@dataclasses.dataclass
class EmojiPickerChar:
    """A type representing a single character in EmojiPicker."""
    # Unicode character.
    string: str
    # Name of the unicode character.
    name: str
    # Search keywords related to the unicode character.
    keywords: List[str] = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class EmojiPickerEmoji:
    """A type representing an emoji/emoticon/symbol in EmojiPicker."""
    # Base Emoji.
    base: EmojiPickerChar
    # Base Emoji's variants and alternative emojis.
    alternates: List[EmojiPickerChar] = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class EmojiPickerGroup:
    """A type representing a group of emoji/emoticon/symbols."""
    # Name of the group.
    group: str
    # List of the emojis in the group.
    emoji: List[EmojiPickerEmoji]
    # Determines If the group is search-only.
    search_only: bool = False


def _convert_snake_case_to_camel_case(snake_case_input: str) -> str:
    """Converts an snake-case string to camel-case.

    Args:
        snake_case_input: String that is snake case.

    Returns:
        An string that is camel-case version of input.

    """
    words = snake_case_input.split('_')
    return words[0] + ''.join(word.title() for word in words[1:])


def _emoji_data_dict_factory(
        data: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    """Implements a dictionary factory for emoji data preparation.

    This factory skips empty keys with empty value. It also converts snake-case
    keys to camel-case.

    Args:
        data: A sequence of (key, value) pairs

    Returns:
        A dictionary created from the input sequence where keys with an empty
            list value are ignored and keys are converted to camel-case.
    """
    return {
        _convert_snake_case_to_camel_case(key): value
        for (key, value) in data if not isinstance(value, list) or value
    }


def _load_emoji_characters_from_files(data_paths: List[str]) -> Set[str]:
    """Loads a set of emoji characters from a list of data file paths.

    Args:
        data_paths: A list of emoji data files.

    Returns:
        The set of emoji unicode characters read from the data.
    """
    emoji_character_set = set()
    for data_path in data_paths:
        with open(data_path, 'r') as data_file:
            emoji_groups = json.load(data_file)
            file_character_set = {
                emoji['base']['string']
                for emoji_group in emoji_groups
                for emoji in emoji_group['emoji']
            }
            emoji_character_set.update(file_character_set)
    return emoji_character_set


def _convert_unicode_ranges_to_emoji_chars(
    unicode_ranges: List[Tuple[int, int]],
    cldr_map: Dict[str, EmojiPickerChar],
    ignore_errors: bool = True
) -> Generator[EmojiPickerChar, None, None]:
    """Converts unicode ranges to `EmojiPickerChar` instances.

    Given a list of unicode ranges, it iterates over all characters in all the
    ranges and creates and yields an instance of `EmojiPickerChar` for each
    one.

    Args:
        unicode_ranges: A list of unicode ranges.
        ignore_errors: If True, any exceptions raised during processing
            unicode characters is silently ignored.

    Raises:
        ValueError: If a unicode character does not exist in the data source
            and `ignore_errors` is true, an exception is raised.

    Yields:
        The converted version of each unicode character in the input ranges.
    """

    LOGGER.info(
        'generating EmojiPickerChar instances for ranges: [%s].',
        ', '.join('(U+{:02x}, U+{:02x})'.format(*rng)
                  for rng in unicode_ranges))

    num_chars = 0
    num_ignored = 0

    # Iterate over the input unicode ranges.
    for (start_code_point, end_code_point) in unicode_ranges:
        LOGGER.debug(
            'generating EmojiPickerChar instances '
            'for range (U+%02x to U+%02x).', start_code_point, end_code_point)

        num_chars += end_code_point + 1 - start_code_point
        # Iterate over all code points in the range.
        for code_point in range(start_code_point, end_code_point + 1):
            try:
                unicode_character = chr(code_point)
                if unicode_character in cldr_map:
                    yield cldr_map[unicode_character]
                else:
                    # For the current code point, create the corresponding
                    # character and lookup its name in the unicodedata. Then,
                    # create an instance of  `EmojiPickerChar` from the data.
                    yield EmojiPickerChar(
                        string=unicode_character,
                        name=unicodedata.name(unicode_character).lower(),
                        keywords=CUSTOM_KEYWORDS.get(unicode_character, []))
            except ValueError:
                # If ignore_errors is False, raise the exception.
                if not ignore_errors:
                    raise
                else:
                    num_ignored += 1
                    LOGGER.warning('invalid code point U+%02x.', code_point)

    LOGGER.info('stats: #returned instances: %d, #ignored code points: %d',
                num_chars, num_ignored)


def get_symbols_groups(
    group_unicode_ranges: Dict[str, List[Tuple[int, int]]],
    cldr_map: Dict[str, EmojiPickerChar],
    search_only: bool = False,
    ignore_errors: bool = True,
    filter_set: Optional[Set[str]] = None
) -> List[EmojiPickerGroup]:
    """Creates symbols data from predefined groups and their unicode ranges.

    Args:
        group_unicode_ranges: A base mapping of group names to unicode ranges.
        search_only: If True, the group is considered search-only.
        ignore_errors: If True, any exceptions raised during processing
            unicode characters is silently ignored.
        filter_set: If not None, the characters that exist in this set are
            excluded from output symbol groups.
        cldr_map: Dictionary of cldr data for symbol chars.

    Raises:
        ValueError: If a unicode character does not exist in the data source
            and `ignore_errors` is true, the exception is raised.
    """

    emoji_groups = list()
    for (group_name, unicode_ranges) in group_unicode_ranges.items():
        LOGGER.info('generating symbols for group %s.', group_name)
        emoji_chars = _convert_unicode_ranges_to_emoji_chars(
            unicode_ranges, ignore_errors=ignore_errors, cldr_map=cldr_map)
        emoji = [
            EmojiPickerEmoji(base=emoji_char) for emoji_char in emoji_chars
            if filter_set is None or emoji_char.string not in filter_set
        ]

        emoji_group = EmojiPickerGroup(group=group_name,
                                       emoji=emoji,
                                       search_only=search_only)
        emoji_groups.append(emoji_group)
    return emoji_groups


def parse_cldr_annotations(keyword_file: str) -> Dict[str, EmojiPickerChar]:
    symbol_to_data: Dict[str, EmojiPickerChar] = {}

    tree = xml.etree.ElementTree.parse(keyword_file)
    root = tree.getroot()

    for tag in root.iterfind('./annotations/annotation[@cp]'):
        cp = tag.get('cp')

        if cp not in symbol_to_data:
            symbol_to_data[cp] = EmojiPickerChar(string=cp, name='')

        if tag.get('type') == 'tts':
            symbol_to_data[cp].name = tag.text
        else:
            symbol_to_data[cp].keywords = tag.text.split(' | ')
    return symbol_to_data


def main(argv: List[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--output',
                        required=True,
                        type=str,
                        help='Path to write the output JSON file.')
    parser.add_argument('--verbose',
                        required=False,
                        default=False,
                        action='store_true',
                        help='Set the logging level to Debug.')
    parser.add_argument('--filter-data-paths', action='append', nargs='+')
    parser.add_argument('--cldr_data_paths',
                        default=[],
                        nargs='*',
                        help='Paths to find CLDR unicode annotations')

    args = parser.parse_args(argv)

    if args.verbose:
        LOGGER.setLevel(level=logging.DEBUG)

    # Flatten list of data paths if any.
    filter_data_paths = list()
    if args.filter_data_paths is not None:
        for data_path_element in args.filter_data_paths:
            filter_data_paths.extend(data_path_element)

    # Loads a list of other emoji characters that must be
    # excluded from symbols.
    filter_set = _load_emoji_characters_from_files(
        data_paths=filter_data_paths)

    # Explicitly remove individual symbols that don't render on ChromeOS
    filter_set |= INVALID_SYMBOLS

    cldr_map: Dict[str, EmojiPickerChar] = {}
    for path in args.cldr_data_paths:
        # Would replace existing symbols if already exists.
        cldr_map.update(parse_cldr_annotations(path))

    # Add symbol groups.
    symbols_groups = get_symbols_groups(group_unicode_ranges=SYMBOLS_GROUPS,
                                        filter_set=filter_set,
                                        search_only=False,
                                        cldr_map=cldr_map)

    # Add search-only symbol groups.
    symbols_groups.extend(
        get_symbols_groups(group_unicode_ranges=SEARCH_ONLY_SYMBOLS_GROUPS,
                           filter_set=filter_set,
                           search_only=True,
                           cldr_map=cldr_map))

    # Create the data and convert them to dict.
    symbols_groups_dicts = []
    for symbol_group in symbols_groups:
        symbol_group_dict = dataclasses.asdict(
            symbol_group, dict_factory=_emoji_data_dict_factory)
        symbols_groups_dicts.append(symbol_group_dict)

    # Write the result to output path as json file.
    with action_helpers.atomic_output(args.output) as tmp_file:
        tmp_file.write(
            json.dumps(symbols_groups_dicts,
                       separators=(',', ':'),
                       ensure_ascii=False).encode('utf-8'))


if __name__ == '__main__':
    main(sys.argv[1:])
