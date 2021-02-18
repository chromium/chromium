# Camera App

Camera App is a packaged app designed to take photos and record videos.

## Supported systems

Chrome OS. Other platforms are not guaranteed to work.

## Installing, packaging, and testing

There is a helper script `utils/cca.py` with a convenient symlink `cca` in the
top directory. Here are some quick examples:

```
# Deploy CCA to <device>
$ ./cca deploy <device>

# Run CCA Tast tests on <device>
$ ./cca test <device> [patterns...]
```

For more details, please check the usage of individual commands with the
`--help` flag.

## Adding files

When adding a file (e.g. CSS/HTML/JS/Sound/Image), please also add the file name
into the list of corresponding .gni file. For example, when adding a "foo.js",
please also add "foo.js" into the list in "js/js.gni".

## Known issues

<https://crbug.com/?q=component%3APlatform%3EApps%3ECamera>
