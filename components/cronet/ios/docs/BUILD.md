# Building Cronet (on iOS)

## Get source and dependencies
### source
- Install depot_tools per https://chromium.googlesource.com/chromium/src/+/master/docs/ios/build_instructions.md
- Make directory for the chromium source, and then fetch:
```
    ~ $ mkdir chromium && cd chromium
    ~/chromium $ fetch --nohooks ios
```

- Enter the ./src directory:
```
    ~/chromium $ cd src
```
### deps
- Download the depenedencies
```
    ~/chromium/src $ gclient sync
```

## Build it!

- We'll be using it a bunch, so you may want to put cr_cronet.py in your path.  Of course, you can just use its full name every time if you want...
```
    ~/chromium/src $ ln -s /path/to/components/cronet/tools/cr_cronet.py /somewhere/in/your/path
```

    or however else you want to do this

This sets up the build directory...
```
    ~/chromium/src $ cr_cronet.py gn
```
...and this builds it!
```
    ~/chromium/src $ cr_cronet.py build -d out/Debug-iphonesimulator
```

- You can also use build-test to run tests on the simulator
```
    ~/chromium/src $ cr_cronet.py build-test -d out/Debug-iphonesimulator
```

- If you want to deploy to hardware, you will have to set up XCode for deploying to hardware, and then use cr_cronet.py gn with the -i flag (for iphoneos build), and cr_cronet.py build with either the -i flag, or using the out/Debug-iphoneos directory.
```
    ~/chromium/src $ cr_cronet.py gn -i
```
and then
```
    ~/chromium/src $ cr_cronet.py build -i
```
or
```
    ~/chromium/src $ cr_cronet.py build -d out/Debug-iphoneos
```

## Updating

- Acquire the most recent version of the source with:
```
    ~/chromium/src $ cr_cronet.py sync
```
and then rebuild:
```
    ~/chromium/src $ cr_cronet.py build -d out/Debug-iphoneos
    ~/chromium/src $ cr_cronet.py build -d out/Debug-iphonesimulator
```

For more information, you can run
```
    ~ $ cr_cronet.py -h
```
