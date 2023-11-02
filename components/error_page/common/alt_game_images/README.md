# How to obfuscate new dino game art before a surprise launch

The tools in this directory make it possible to add art to the dino game but
keep it hidden until a planned reveal.

The images are built into Chrome in an obfuscated form so that they can't be
viewed if they're found in the source tree or in the binary. When it's time to
launch, a secret key can be sent to clients via feature param. If the key is
present, images are deobfuscated at net error page load time and made available
to the dino game.

Warning: although this procedure has flavors of cryptography, it should _never_
be used to protect information that matters!

## Generate a key

Run:

```
python3 generate_key.py > /tmp/dino_key
```

Keep the key somewhere safe so that it can remain secret until launch. (An
internal repo or doc is probably good enough.)

## Generate `alt_game_images.cc`

You'll need to install Pycryptodome, jinja2, and secrets:
`pip install --user pycryptodome jinja2 secrets`

Run `create_alt_game_images_cc.py` as shown below (filling in your own values as
needed):

```
# Paths to directory containing input images.
IMAGES_DIR=path/to/images

# Names of images in the order in which they should be added to the
# generated source. They correspond to files named surfing_1x.png,
# surfing_2x.png, hurdles_1x.png, etc.
IMAGE_NAMES="common,equestrian,gymnastics,hurdles,surfing,swimming"

# The key you generated in the first step.
KEY=$(cat /tmp/dino_key)

python3 create_alt_game_images_cc.py \
  --images_dir=$IMAGES_DIR \
  --image_names=$IMAGE_NAMES \
  --key=$KEY

git cl format
```

# Make your changes to game code

The images are made available to net error page Javascript in
`LocalizedError::GetPageState()`: see
https://source.chromium.org/chromium/chromium/src/+/main:components/error_page/common/localized_error.cc;drc=4e22ca0649a83df9026440bf8a4c2c04d1de8633;l=922.

An image data URL passed to the page with
`result.strings.SetStringPath("someImage", ...)` will be available in Javascript
as `loadTimeData.someImage`.

# Debug

You can try loading the images by running Chrome with the flag and key set on
the command line:

```
out/Debug/chrome --enable-features=NetErrorAltGameMode:Key/$KEY
```

and then bringing up the net error page somehow (chrome://network-error/-106).

If all is well, there should be no logs that mention `alt_game_images.cc`.

# Launch

The new images can be released to users by enabling the feature
"NetErrorAltGameMode" and setting feature param "Key" to the key.
