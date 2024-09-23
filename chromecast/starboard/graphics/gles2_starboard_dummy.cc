// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dummy implementation of gles2_starboard.h. This can be used to compile
// without starboard headers. It should never be used in production.
//
// TODO(b/333131992): remove this

#include "chromecast/starboard/graphics/gles2_starboard.h"

extern "C" {

GL_APICALL void GL_APIENTRY Sb_glActiveTexture(GLenum texture) {}

GL_APICALL void GL_APIENTRY Sb_glAttachShader(GLuint program, GLuint shader) {}

GL_APICALL void GL_APIENTRY Sb_glBindAttribLocation(GLuint program,
                                                    GLuint index,
                                                    const GLchar* name) {}

GL_APICALL void GL_APIENTRY Sb_glBindBuffer(GLenum target, GLuint buffer) {}

GL_APICALL void GL_APIENTRY Sb_glBindFramebuffer(GLenum target,
                                                 GLuint framebuffer) {}

GL_APICALL void GL_APIENTRY Sb_glBindRenderbuffer(GLenum target,
                                                  GLuint renderbuffer) {}

GL_APICALL void GL_APIENTRY Sb_glBindTexture(GLenum target, GLuint texture) {}

GL_APICALL void GL_APIENTRY Sb_glBlendColor(GLfloat red,
                                            GLfloat green,
                                            GLfloat blue,
                                            GLfloat alpha) {}

GL_APICALL void GL_APIENTRY Sb_glBlendEquation(GLenum mode) {}

GL_APICALL void GL_APIENTRY Sb_glBlendEquationSeparate(GLenum modeRGB,
                                                       GLenum modeAlpha) {}

GL_APICALL void GL_APIENTRY Sb_glBlendFunc(GLenum sfactor, GLenum dfactor) {}

GL_APICALL void GL_APIENTRY Sb_glBlendFuncSeparate(GLenum sfactorRGB,
                                                   GLenum dfactorRGB,
                                                   GLenum sfactorAlpha,
                                                   GLenum dfactorAlpha) {}

GL_APICALL void GL_APIENTRY Sb_glBufferData(GLenum target,
                                            GLsizeiptr size,
                                            const void* data,
                                            GLenum usage) {}

GL_APICALL void GL_APIENTRY Sb_glBufferSubData(GLenum target,
                                               GLintptr offset,
                                               GLsizeiptr size,
                                               const void* data) {}

GL_APICALL GLenum GL_APIENTRY Sb_glCheckFramebufferStatus(GLenum target) {
  return 0;
}

GL_APICALL void GL_APIENTRY Sb_glClear(GLbitfield mask) {}

GL_APICALL void GL_APIENTRY Sb_glClearColor(GLfloat red,
                                            GLfloat green,
                                            GLfloat blue,
                                            GLfloat alpha) {}

GL_APICALL void GL_APIENTRY Sb_glClearDepthf(GLfloat d) {}

GL_APICALL void GL_APIENTRY Sb_glClearStencil(GLint s) {}

GL_APICALL void GL_APIENTRY Sb_glColorMask(GLboolean red,
                                           GLboolean green,
                                           GLboolean blue,
                                           GLboolean alpha) {}

GL_APICALL void GL_APIENTRY Sb_glCompileShader(GLuint shader) {}

GL_APICALL void GL_APIENTRY Sb_glCompressedTexImage2D(GLenum target,
                                                      GLint level,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLint border,
                                                      GLsizei imageSize,
                                                      const void* data) {}

GL_APICALL void GL_APIENTRY Sb_glCompressedTexSubImage2D(GLenum target,
                                                         GLint level,
                                                         GLint xoffset,
                                                         GLint yoffset,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLenum format,
                                                         GLsizei imageSize,
                                                         const void* data) {}

GL_APICALL void GL_APIENTRY Sb_glCopyTexImage2D(GLenum target,
                                                GLint level,
                                                GLenum internalformat,
                                                GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border) {}

GL_APICALL void GL_APIENTRY Sb_glCopyTexSubImage2D(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLint x,
                                                   GLint y,
                                                   GLsizei width,
                                                   GLsizei height) {}

GL_APICALL GLuint GL_APIENTRY Sb_glCreateProgram(void) {
  return 0;
}

GL_APICALL GLuint GL_APIENTRY Sb_glCreateShader(GLenum type) {
  return 0;
}

GL_APICALL void GL_APIENTRY Sb_glCullFace(GLenum mode) {}

GL_APICALL void GL_APIENTRY Sb_glDeleteBuffers(GLsizei n,
                                               const GLuint* buffers) {}

GL_APICALL void GL_APIENTRY
Sb_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {}

GL_APICALL void GL_APIENTRY Sb_glDeleteProgram(GLuint program) {}

GL_APICALL void GL_APIENTRY
Sb_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {}

GL_APICALL void GL_APIENTRY Sb_glDeleteShader(GLuint shader) {}

GL_APICALL void GL_APIENTRY Sb_glDeleteTextures(GLsizei n,
                                                const GLuint* textures) {}

GL_APICALL void GL_APIENTRY Sb_glDepthFunc(GLenum func) {}

GL_APICALL void GL_APIENTRY Sb_glDepthMask(GLboolean flag) {}

GL_APICALL void GL_APIENTRY Sb_glDepthRangef(GLfloat n, GLfloat f) {}

GL_APICALL void GL_APIENTRY Sb_glDetachShader(GLuint program, GLuint shader) {}

GL_APICALL void GL_APIENTRY Sb_glDisable(GLenum cap) {}

GL_APICALL void GL_APIENTRY Sb_glDisableVertexAttribArray(GLuint index) {}

GL_APICALL void GL_APIENTRY Sb_glDrawArrays(GLenum mode,
                                            GLint first,
                                            GLsizei count) {}

GL_APICALL void GL_APIENTRY Sb_glDrawElements(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices) {}

GL_APICALL void GL_APIENTRY Sb_glEnable(GLenum cap) {}

GL_APICALL void GL_APIENTRY Sb_glEnableVertexAttribArray(GLuint index) {}

GL_APICALL void GL_APIENTRY Sb_glFinish(void) {}

GL_APICALL void GL_APIENTRY Sb_glFlush(void) {}

GL_APICALL void GL_APIENTRY
Sb_glFramebufferRenderbuffer(GLenum target,
                             GLenum attachment,
                             GLenum renderbuffertarget,
                             GLuint renderbuffer) {}

GL_APICALL void GL_APIENTRY Sb_glFramebufferTexture2D(GLenum target,
                                                      GLenum attachment,
                                                      GLenum textarget,
                                                      GLuint texture,
                                                      GLint level) {}

GL_APICALL void GL_APIENTRY Sb_glFrontFace(GLenum mode) {}

GL_APICALL void GL_APIENTRY Sb_glGenBuffers(GLsizei n, GLuint* buffers) {}

GL_APICALL void GL_APIENTRY Sb_glGenerateMipmap(GLenum target) {}

GL_APICALL void GL_APIENTRY Sb_glGenFramebuffers(GLsizei n,
                                                 GLuint* framebuffers) {}

GL_APICALL void GL_APIENTRY Sb_glGenRenderbuffers(GLsizei n,
                                                  GLuint* renderbuffers) {}

GL_APICALL void GL_APIENTRY Sb_glGenTextures(GLsizei n, GLuint* textures) {}

GL_APICALL void GL_APIENTRY Sb_glGetActiveAttrib(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* size,
                                                 GLenum* type,
                                                 GLchar* name) {}

GL_APICALL void GL_APIENTRY Sb_glGetActiveUniform(GLuint program,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* size,
                                                  GLenum* type,
                                                  GLchar* name) {}

GL_APICALL void GL_APIENTRY Sb_glGetAttachedShaders(GLuint program,
                                                    GLsizei maxCount,
                                                    GLsizei* count,
                                                    GLuint* shaders) {}

GL_APICALL GLint GL_APIENTRY Sb_glGetAttribLocation(GLuint program,
                                                    const GLchar* name) {
  return 0;
}

GL_APICALL void GL_APIENTRY Sb_glGetBooleanv(GLenum pname, GLboolean* data) {}

GL_APICALL void GL_APIENTRY Sb_glGetBufferParameteriv(GLenum target,
                                                      GLenum pname,
                                                      GLint* params) {}

GL_APICALL GLenum GL_APIENTRY Sb_glGetError(void) {
  return 0;
}

GL_APICALL void GL_APIENTRY Sb_glGetFloatv(GLenum pname, GLfloat* data) {}

GL_APICALL void GL_APIENTRY
Sb_glGetFramebufferAttachmentParameteriv(GLenum target,
                                         GLenum attachment,
                                         GLenum pname,
                                         GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetIntegerv(GLenum pname, GLint* data) {}

GL_APICALL void GL_APIENTRY Sb_glGetProgramiv(GLuint program,
                                              GLenum pname,
                                              GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetProgramInfoLog(GLuint program,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLchar* infoLog) {}

GL_APICALL void GL_APIENTRY Sb_glGetRenderbufferParameteriv(GLenum target,
                                                            GLenum pname,
                                                            GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetShaderiv(GLuint shader,
                                             GLenum pname,
                                             GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetShaderInfoLog(GLuint shader,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLchar* infoLog) {}

GL_APICALL void GL_APIENTRY Sb_glGetShaderPrecisionFormat(GLenum shadertype,
                                                          GLenum precisiontype,
                                                          GLint* range,
                                                          GLint* precision) {}

GL_APICALL void GL_APIENTRY Sb_glGetShaderSource(GLuint shader,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLchar* source) {}

GL_APICALL const GLubyte* GL_APIENTRY Sb_glGetString(GLenum name) {
  return nullptr;
}

GL_APICALL void GL_APIENTRY Sb_glGetTexParameterfv(GLenum target,
                                                   GLenum pname,
                                                   GLfloat* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetTexParameteriv(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetUniformfv(GLuint program,
                                              GLint location,
                                              GLfloat* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetUniformiv(GLuint program,
                                              GLint location,
                                              GLint* params) {}

GL_APICALL GLint GL_APIENTRY Sb_glGetUniformLocation(GLuint program,
                                                     const GLchar* name) {
  return 0;
}

GL_APICALL void GL_APIENTRY Sb_glGetVertexAttribfv(GLuint index,
                                                   GLenum pname,
                                                   GLfloat* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetVertexAttribiv(GLuint index,
                                                   GLenum pname,
                                                   GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glGetVertexAttribPointerv(GLuint index,
                                                         GLenum pname,
                                                         void** pointer) {}

GL_APICALL void GL_APIENTRY Sb_glHint(GLenum target, GLenum mode) {}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsBuffer(GLuint buffer) {
  return 0;
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsEnabled(GLenum cap) {
  return 0;
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsFramebuffer(GLuint framebuffer) {
  return 0;
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsProgram(GLuint program) {
  return 0;
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsRenderbuffer(GLuint renderbuffer) {
  return 0;
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsShader(GLuint shader) {
  return 0;
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsTexture(GLuint texture) {
  return 0;
}

GL_APICALL void GL_APIENTRY Sb_glLineWidth(GLfloat width) {}

GL_APICALL void GL_APIENTRY Sb_glLinkProgram(GLuint program) {}

GL_APICALL void GL_APIENTRY Sb_glPixelStorei(GLenum pname, GLint param) {}

GL_APICALL void GL_APIENTRY Sb_glPolygonOffset(GLfloat factor, GLfloat units) {}

GL_APICALL void GL_APIENTRY Sb_glReadPixels(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            void* pixels) {}

GL_APICALL void GL_APIENTRY Sb_glReleaseShaderCompiler(void) {}

GL_APICALL void GL_APIENTRY Sb_glRenderbufferStorage(GLenum target,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height) {}

GL_APICALL void GL_APIENTRY Sb_glSampleCoverage(GLfloat value,
                                                GLboolean invert) {}

GL_APICALL void GL_APIENTRY Sb_glScissor(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height) {}

GL_APICALL void GL_APIENTRY Sb_glShaderBinary(GLsizei count,
                                              const GLuint* shaders,
                                              GLenum binaryformat,
                                              const void* binary,
                                              GLsizei length) {}

GL_APICALL void GL_APIENTRY Sb_glShaderSource(GLuint shader,
                                              GLsizei count,
                                              const GLchar* const* string,
                                              const GLint* length) {}

GL_APICALL void GL_APIENTRY Sb_glStencilFunc(GLenum func,
                                             GLint ref,
                                             GLuint mask) {}

GL_APICALL void GL_APIENTRY Sb_glStencilFuncSeparate(GLenum face,
                                                     GLenum func,
                                                     GLint ref,
                                                     GLuint mask) {}

GL_APICALL void GL_APIENTRY Sb_glStencilMask(GLuint mask) {}

GL_APICALL void GL_APIENTRY Sb_glStencilMaskSeparate(GLenum face, GLuint mask) {
}

GL_APICALL void GL_APIENTRY Sb_glStencilOp(GLenum fail,
                                           GLenum zfail,
                                           GLenum zpass) {}

GL_APICALL void GL_APIENTRY Sb_glStencilOpSeparate(GLenum face,
                                                   GLenum sfail,
                                                   GLenum dpfail,
                                                   GLenum dppass) {}

GL_APICALL void GL_APIENTRY Sb_glTexImage2D(GLenum target,
                                            GLint level,
                                            GLint internalformat,
                                            GLsizei width,
                                            GLsizei height,
                                            GLint border,
                                            GLenum format,
                                            GLenum type,
                                            const void* pixels) {}

GL_APICALL void GL_APIENTRY Sb_glTexParameterf(GLenum target,
                                               GLenum pname,
                                               GLfloat param) {}

GL_APICALL void GL_APIENTRY Sb_glTexParameterfv(GLenum target,
                                                GLenum pname,
                                                const GLfloat* params) {}

GL_APICALL void GL_APIENTRY Sb_glTexParameteri(GLenum target,
                                               GLenum pname,
                                               GLint param) {}

GL_APICALL void GL_APIENTRY Sb_glTexParameteriv(GLenum target,
                                                GLenum pname,
                                                const GLint* params) {}

GL_APICALL void GL_APIENTRY Sb_glTexSubImage2D(GLenum target,
                                               GLint level,
                                               GLint xoffset,
                                               GLint yoffset,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {}

GL_APICALL void GL_APIENTRY Sb_glUniform1f(GLint location, GLfloat v0) {}

GL_APICALL void GL_APIENTRY Sb_glUniform1fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniform1i(GLint location, GLint v0) {}

GL_APICALL void GL_APIENTRY Sb_glUniform1iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniform2f(GLint location,
                                           GLfloat v0,
                                           GLfloat v1) {}

GL_APICALL void GL_APIENTRY Sb_glUniform2fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniform2i(GLint location, GLint v0, GLint v1) {
}

GL_APICALL void GL_APIENTRY Sb_glUniform2iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniform3f(GLint location,
                                           GLfloat v0,
                                           GLfloat v1,
                                           GLfloat v2) {}

GL_APICALL void GL_APIENTRY Sb_glUniform3fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniform3i(GLint location,
                                           GLint v0,
                                           GLint v1,
                                           GLint v2) {}

GL_APICALL void GL_APIENTRY Sb_glUniform3iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {}

GL_APICALL void GL_APIENTRY
Sb_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
}

GL_APICALL void GL_APIENTRY Sb_glUniform4fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {}

GL_APICALL void GL_APIENTRY
Sb_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {}

GL_APICALL void GL_APIENTRY Sb_glUniform4iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniformMatrix2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniformMatrix3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {}

GL_APICALL void GL_APIENTRY Sb_glUniformMatrix4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {}

GL_APICALL void GL_APIENTRY Sb_glUseProgram(GLuint program) {}

GL_APICALL void GL_APIENTRY Sb_glValidateProgram(GLuint program) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib1f(GLuint index, GLfloat x) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib1fv(GLuint index,
                                                 const GLfloat* v) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib2f(GLuint index,
                                                GLfloat x,
                                                GLfloat y) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib2fv(GLuint index,
                                                 const GLfloat* v) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib3f(GLuint index,
                                                GLfloat x,
                                                GLfloat y,
                                                GLfloat z) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib3fv(GLuint index,
                                                 const GLfloat* v) {}

GL_APICALL void GL_APIENTRY
Sb_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib4fv(GLuint index,
                                                 const GLfloat* v) {}

GL_APICALL void GL_APIENTRY Sb_glVertexAttribPointer(GLuint index,
                                                     GLint size,
                                                     GLenum type,
                                                     GLboolean normalized,
                                                     GLsizei stride,
                                                     const void* pointer) {}

GL_APICALL void GL_APIENTRY Sb_glViewport(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height) {}
}
