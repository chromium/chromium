#!/usr/bin/env python/2/3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import app
from absl import flags
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
import base64
import os
from jinja2 import Environment, FileSystemLoader, select_autoescape

FLAGS = flags.FLAGS
flags.DEFINE_string('images_dir', None, 'Directory containing input images.')
flags.DEFINE_list(
    'image_names', None,
    'Comma-separated list of image names in the order in which they should ' +
    'be included in the generated source.')
flags.DEFINE_string('key', None,
                    'Base64url-encoded key used to obfuscate images.')
flags.mark_flags_as_required(['images_dir', 'image_names', 'key'])

_TEMPLATE_FILE_NAME = 'alt_game_images.cc.template'
_README_FILE = 'README.md'


class Image:
    def __init__(self, name, literals, size):
        self.name = name
        self.literals = literals
        self.size = size


def ObfuscateImage(name, key_bytes, image_bytes):
    # WARNING: for obfuscation only. A CBC IV should never be taken from the
    # key material.
    cipher = AES.new(key_bytes, AES.MODE_CBC, iv=key_bytes[:16])
    ciphertext_bytes = cipher.encrypt(pad(image_bytes, AES.block_size))

    ciphertext_hex_literals = []
    for b in ciphertext_bytes:
        ciphertext_hex_literals.append(hex(b))
    return Image(name, ','.join(ciphertext_hex_literals),
                 len(ciphertext_bytes))


def ConvertToDataUrl(image_bytes):
    return b'data:image/png;base64,' + base64.b64encode(image_bytes)


def ObfuscateImages(key_bytes, images_dir, image_suffix):
    if len(FLAGS.image_names) > 26:
        raise RuntimeError("We aren't prepared to handle more than 26 images.")

    result = []
    for i, image_name in enumerate(FLAGS.image_names):
        name = chr(ord('A') + i)
        image_bytes = None
        with open(
                os.path.join(images_dir, image_name) + image_suffix + '.png',
                'rb') as file:
            image_bytes = file.read()
        image_bytes = ConvertToDataUrl(image_bytes)
        result.append(ObfuscateImage(name, key_bytes, image_bytes))

    return result


def CreateAltGameImagesCc(images_1x, images_2x, script_dir):
    template_dir_path = script_dir
    env = Environment(loader=FileSystemLoader(template_dir_path),
                      autoescape=select_autoescape())
    template = env.get_template(_TEMPLATE_FILE_NAME)
    rendered = template.render(images_1x=images_1x,
                               images_2x=images_2x,
                               template_file='alt_game_images/' +
                               _TEMPLATE_FILE_NAME,
                               readme_file=_README_FILE)

    with open(os.path.join(script_dir, '../alt_game_images.cc'),
              'w') as out_file:
        out_file.write(rendered)


def main(argv):
    script_dir = os.path.dirname(os.path.abspath(__file__))

    key_bytes = base64.urlsafe_b64decode(FLAGS.key + '==')
    if len(key_bytes) != 32:
        raise RuntimeError('Key length must be 32.')

    obfuscated_images_1x = ObfuscateImages(key_bytes, FLAGS.images_dir, '_1x')
    obfuscated_images_2x = ObfuscateImages(key_bytes, FLAGS.images_dir, '_2x')
    CreateAltGameImagesCc(obfuscated_images_1x, obfuscated_images_2x,
                          script_dir)


if __name__ == '__main__':
    app.run(main)
