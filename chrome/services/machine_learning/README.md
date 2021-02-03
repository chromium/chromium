# Chrome Machine Learning Service

This is a service for sandboxed evaluations of machine learning models.
([Design doc](https://docs.google.com/document/d/1i5uSTFe3uKwHifVQ0aFs6kYfsGlCt_ZGCiOgqcYM0Tg/edit?usp=sharing))


To build Chrome with TFLite library follow these instructions. For the unit test we use a [simple tflite model](../../test/data/simple_test.tflite). This is a simple sequential model as following:

```python
input_shape = (32, 32, 3)
model = tf.keras.models.Sequential([
    tf.keras.Input(shape=input_shape, dtype=np.float32),
    tf.keras.layers.Conv2D(16, 3, strides=(1, 1), activation='relu', padding='same', 
    input_shape=input_shape),
    tf.keras.layers.MaxPooling2D((2, 2)),
    tf.keras.layers.Flatten(),
    tf.keras.layers.Dense(10),
])
```

Build Tensorflow Lite library:

  clone https://github.com/tensorflow/tensorflow

  cd tensorflow

  for x86 architecture:
    
    bazel build tensorflow/lite/c:libtensorflowlite_c.so

  for android:

    bazel build --config=android_arm64 tensorflow/lite/c:libtensorflowlite_c.so

  copy 'libtensorflowlite_c.so' file to chromium/src/third_party/tensorflow

  link the library to a soft link in system library directory under /lib/

Copy libraries:
  
  c_api.h and common.h [here](https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/c) to into third_party/tensorflow/lite/c

Build TFLite in chrome:
  
  Set flag build_with_tflite_lib=true
  
  Uncomment thirdparty library in [machine learning header file](./machine_learning_tflite_predictor.h).
  